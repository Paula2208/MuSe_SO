// indexador.c corregido
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

#define MAX_FIELD 128
#define MAX_SEEDS 10
#define MAX_ARTIST_BUCKETS 211
#define INDEX_FOLDER "./output/emotions/"
#define LINE_BUFFER 4096
#define NUM_FIELDS 12

typedef struct PosNode {
    long pos;
    struct PosNode *next;
} PosNode;

typedef struct ArtistNode {
    char artist[MAX_FIELD];
    PosNode *positions;
    struct ArtistNode *next;
} ArtistNode;

typedef struct {
    ArtistNode *buckets[MAX_ARTIST_BUCKETS];
} ArousalIndex;

typedef struct EmotionIndex {
    char emotion[MAX_FIELD];
    ArousalIndex *arousals;
    struct EmotionIndex *next;
} EmotionIndex;

EmotionIndex *emotion_index_head = NULL;

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
    for (EmotionIndex *curr = emotion_index_head; curr; curr = curr->next)
        if (strcmp(curr->emotion, emotion) == 0) return curr;

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
        printf("[indexador] Índice guardado para emoción '%s'\n", curr->emotion);
    }
}

void buildIndex(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo CSV");
        exit(1);
    }

    char line[LINE_BUFFER];
    if (!fgets(line, sizeof(line), file)) {
        fprintf(stderr, "Error leyendo línea (posiblemente EOF inesperado)\n");
        fclose(file);
        return;
    }

    long pos = ftell(file);
    long count = 0;

    while (fgets(line, sizeof(line), file)) {
        count++;

        char *tokens[NUM_FIELDS];
        char *p = line;
        for (int i = 0; i < NUM_FIELDS; i++) {
            char *start = p;
            while (*p && *p != ',') {
                if (*p == '[') while (*p && *p != ']') p++;
                if (*p) p++;
            }
            if (*p) *p++ = '\0';
            tokens[i] = start;
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

        pos = ftell(file);
    }

    fclose(file);
    printf("[indexador] Total de canciones procesadas: %ld\n", count);
    save_index_to_disk();
}
