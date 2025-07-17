#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>

#define MAX_FIELD 128
#define MAX_SEEDS 10
#define MAX_ARTIST_BUCKETS 211
#define INDEX_FOLDER "./output/emotions/"
#define LINE_BUFFER 4096
#define NUM_FIELDS 12
#define CHUNK_SIZE 500000
#define NUM_THREADS 8

// Declaraciones adelantadas
typedef struct PosNode PosNode;
typedef struct ArtistNode ArtistNode;

// Definiciones completas
struct PosNode {
    long pos;
    PosNode *next;
};

struct ArtistNode {
    char artist[MAX_FIELD];
    PosNode *positions;
    ArtistNode *next;
};

typedef struct {
    ArtistNode *buckets[MAX_ARTIST_BUCKETS];
} ArousalIndex;

typedef struct EmotionIndex {
    char emotion[MAX_FIELD];
    ArousalIndex *arousals;
    struct EmotionIndex *next;
} EmotionIndex;

typedef struct {
    int id;
    char **lines;
    long *positions;
    long num_lines;
} ThreadArgs;

typedef struct {
    long id;
    char title[MAX_FIELD];
    char artist[MAX_FIELD];
    char url[MAX_FIELD];
    char emotions[MAX_SEEDS][MAX_FIELD];
    int num_seeds;
    float valence;
    float arousal;
    float dominance;
    char genre[MAX_FIELD];
} Song;

// Global index
EmotionIndex *emotion_index_head = NULL;
pthread_mutex_t index_mutex = PTHREAD_MUTEX_INITIALIZER;

// ------------- FUNCIONES AUXILIARES -------------

void sanitize_input(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (isalpha((unsigned char)*src))
            *dst++ = tolower((unsigned char)*src);
        src++;
    }
    *dst = '\0';
}

unsigned int hash_artist(const char *key) {
    unsigned int hash = 0;
    while (*key)
        hash = (hash << 5) + tolower(*key++);
    return hash % MAX_ARTIST_BUCKETS;
}

EmotionIndex *get_or_create_emotion(const char *emotion) {
    EmotionIndex *curr;
    for (curr = emotion_index_head; curr; curr = curr->next)
        if (strcmp(curr->emotion, emotion) == 0)
            return curr;

    EmotionIndex *new = malloc(sizeof(EmotionIndex));
    strncpy(new->emotion, emotion, MAX_FIELD - 1);
    new->emotion[MAX_FIELD - 1] = '\0';
    new->arousals = calloc(101, sizeof(ArousalIndex));
    new->next = emotion_index_head;
    emotion_index_head = new;
    return new;
}

void add_position(const char *emotion, int arousal, const char *artist, long pos) {
    if (arousal < 0 || arousal > 100) return;

    pthread_mutex_lock(&index_mutex);
    EmotionIndex *eidx = get_or_create_emotion(emotion);
    ArousalIndex *ai = &eidx->arousals[arousal];
    unsigned int idx = hash_artist(artist);

    ArtistNode *curr = ai->buckets[idx];
    while (curr && strcmp(curr->artist, artist) != 0)
        curr = curr->next;

    if (!curr) {
        curr = malloc(sizeof(ArtistNode));
        strncpy(curr->artist, artist, MAX_FIELD - 1);
        curr->artist[MAX_FIELD - 1] = '\0';
        curr->positions = NULL;
        curr->next = ai->buckets[idx];
        ai->buckets[idx] = curr;
    }

    PosNode *p = malloc(sizeof(PosNode));
    p->pos = pos;
    p->next = curr->positions;
    curr->positions = p;
    pthread_mutex_unlock(&index_mutex);
}

void save_index_to_disk() {
    mkdir(INDEX_FOLDER, 0775);
    for (EmotionIndex *curr = emotion_index_head; curr; curr = curr->next) {
        char path[256];
        snprintf(path, sizeof(path), "%sindex_%s.bin", INDEX_FOLDER, curr->emotion);
        FILE *f = fopen(path, "wb");
        if (!f) {
            perror("fopen");
            continue;
        }

        for (int i = 0; i <= 100; i++) {
            ArousalIndex *ai = &curr->arousals[i];
            int bucket_count = 0;
            long pos = ftell(f);
            fwrite(&bucket_count, sizeof(int), 1, f); // Placeholder

            for (int b = 0; b < MAX_ARTIST_BUCKETS; b++) {
                for (ArtistNode *an = ai->buckets[b]; an; an = an->next) {
                    int len = strlen(an->artist);
                    fwrite(&len, sizeof(int), 1, f);
                    fwrite(an->artist, sizeof(char), len, f);

                    int count = 0;
                    for (PosNode *pn = an->positions; pn; pn = pn->next) count++;
                    fwrite(&count, sizeof(int), 1, f);
                    for (PosNode *pn = an->positions; pn; pn = pn->next)
                        fwrite(&pn->pos, sizeof(long), 1, f);

                    bucket_count++;
                }
            }

            long current = ftell(f);
            fseek(f, pos, SEEK_SET);
            fwrite(&bucket_count, sizeof(int), 1, f);
            fseek(f, current, SEEK_SET);
        }

        fclose(f);
        
    }
    printf("[indexador] Índice guardado.\n");
}

void *process_lines(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    for (long i = 0; i < args->num_lines; i++) {
        char *line = args->lines[i];
        long pos = args->positions[i];

        char *tokens[NUM_FIELDS];
        char *p = line;
        for (int j = 0; j < NUM_FIELDS; j++) {
            char *start = p;
            while (*p && *p != ',') {
                if (*p == '[') while (*p && *p != ']') p++;
                if (*p) p++;
            }
            if (*p) *p++ = '\0';
            tokens[j] = start;
        }

        char artist[MAX_FIELD] = "";
        if (tokens[2]) {
            strncpy(artist, tokens[2], MAX_FIELD - 1);
            artist[MAX_FIELD - 1] = '\0';
            sanitize_input(artist);
        }

        if (tokens[3]) {
            char seeds_raw[MAX_FIELD];
            strncpy(seeds_raw, tokens[3], MAX_FIELD - 1);
            seeds_raw[MAX_FIELD - 1] = '\0';
            char *q = seeds_raw;

            while ((q = strchr(q, '\'')) != NULL) {
                q++;
                char *end = strchr(q, '\'');
                if (!end) break;
                *end = '\0';

                char emotion_clean[MAX_FIELD];
                strncpy(emotion_clean, q, MAX_FIELD - 1);
                emotion_clean[MAX_FIELD - 1] = '\0';
                sanitize_input(emotion_clean);

                if (strlen(emotion_clean) > 0) {
                    int arousal = (int) atof(tokens[6]);
                    add_position(emotion_clean, arousal, artist, pos);
                }

                q = end + 1;
            }
        }
    }

    return NULL;
}

// ------------- INDEXADOR PRINCIPAL -------------

void buildIndex(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo CSV");
        exit(1);
    }

    char header[LINE_BUFFER];
    if(!fgets(header, sizeof(header), file)){
        perror("Error leyendo el header del CSV");
        fclose(file);
        exit(1);
    } // Skip header

    char **lines = malloc(sizeof(char *) * CHUNK_SIZE);
    long *positions = malloc(sizeof(long) * CHUNK_SIZE);
    long total = 0;

    long chunk_id = 0;
    long count = 0;
    char line[LINE_BUFFER];

    while (1) {
        long pos = ftell(file);
        if (!fgets(line, sizeof(line), file)) break;

        lines[count] = strdup(line);
        positions[count] = pos;
        count++;
        total++;


        if (count >= CHUNK_SIZE) {
            printf("[indexador] Procesando chunk %ld...\n", ++chunk_id);

            pthread_t threads[NUM_THREADS];
            ThreadArgs args[NUM_THREADS];
            long chunk_per_thread = count / NUM_THREADS;

            for (int i = 0; i < NUM_THREADS; i++) {
                args[i].id = i;
                args[i].lines = &lines[i * chunk_per_thread];
                args[i].positions = &positions[i * chunk_per_thread];
                args[i].num_lines = (i == NUM_THREADS - 1) ? (count - i * chunk_per_thread) : chunk_per_thread;
            }

            for (int i = 0; i < NUM_THREADS; i++) {
                pthread_create(&threads[i], NULL, process_lines, &args[i]);
            }

            for (int i = 0; i < NUM_THREADS; i++)
                pthread_join(threads[i], NULL);

            for (int i = 0; i < count; i++) free(lines[i]);
            count = 0;

            save_index_to_disk();
        }
    }

    // Procesar últimas líneas si quedaron
    if (count > 0) {
        printf("[indexador] Procesando último chunk...\n");
        pthread_t threads[NUM_THREADS];
        ThreadArgs args[NUM_THREADS];
        long chunk_per_thread = count / NUM_THREADS;

        for (int i = 0; i < NUM_THREADS; i++) {
            args[i].id = i;
            args[i].lines = &lines[i * chunk_per_thread];
            args[i].positions = &positions[i * chunk_per_thread];
            args[i].num_lines = (i == NUM_THREADS - 1) ? (count - i * chunk_per_thread) : chunk_per_thread;
        }

        for (int i = 0; i < NUM_THREADS; i++) {
           pthread_create(&threads[i], NULL, process_lines, &args[i]);
        }

        for (int i = 0; i < NUM_THREADS; i++)
            pthread_join(threads[i], NULL);

        for (int i = 0; i < count; i++) free(lines[i]);
    }

    free(lines);
    free(positions);
    fclose(file);

    printf("[indexador] Total de canciones procesadas: %ld\n", total);
    save_index_to_disk();
}
