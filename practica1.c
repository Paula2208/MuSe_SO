// practica1.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_FIELD 256
#define MAX_SEEDS 10
#define HASH_SIZE 1000
#define LINE_BUFFER 2048
#define PIPE_REQ "/tmp/search_req"
#define PIPE_RES "/tmp/search_res"

typedef struct {
    char lastfm_url[MAX_FIELD];
    char track[MAX_FIELD];
    char artist[MAX_FIELD];
    char seeds[MAX_SEEDS][MAX_FIELD];
    int seed_count;
    char genre[MAX_FIELD];
    double number_of_emotions;
    double valence_tags;
    double arousal_tags;
    double dominance_tags;
} Song;

typedef struct HashNode {
    long position;
    struct HashNode* next;
} HashNode;

typedef struct {
    char key[MAX_FIELD];
    HashNode* head;
} HashBucket;

HashBucket hashTable[HASH_SIZE];

unsigned int hash(const char *str) {
    unsigned int hash = 0;
    while (*str)
        hash = (hash << 5) + *str++;
    return hash % HASH_SIZE;
}

void insertHash(const char *key, long pos) {
    unsigned int idx = hash(key);
    HashNode *newNode = malloc(sizeof(HashNode));
    if (!newNode) {
        perror("malloc");
        exit(1);
    }
    newNode->position = pos;
    newNode->next = hashTable[idx].head;
    hashTable[idx].head = newNode;
    snprintf(hashTable[idx].key, sizeof(hashTable[idx].key), "%s", key);
}

void clearHash() {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode *curr = hashTable[i].head;
        while (curr) {
            HashNode *temp = curr;
            curr = curr->next;
            free(temp);
        }
        hashTable[i].head = NULL;
    }
}

void buildIndex(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("No se pudo abrir el archivo");
        exit(1);
    }

    char line[LINE_BUFFER];
    long pos = ftell(file);
    if (!fgets(line, sizeof(line), file)) {
        perror("Error leyendo encabezado");
        fclose(file);
        return;
    }

    printf("[indexer] Comenzando indexación del archivo CSV...\n");

    while (fgets(line, sizeof(line), file)) {
        char *tokens[12];
        char *token = strtok(line, ",");
        int i = 0;

        while (token && i < 12) {
            tokens[i++] = token;
            token = strtok(NULL, ",");
        }

        if (i >= 4 && tokens[3]) {
            char seeds_raw[MAX_FIELD];
            snprintf(seeds_raw, sizeof(seeds_raw), "%s", tokens[3]);
            char *seed = strtok(seeds_raw, "[]' ");
            while (seed) {
                insertHash(seed, pos);
                seed = strtok(NULL, ",' ");
            }
        }

        pos = ftell(file);
    }

    fclose(file);
    printf("[indexer] Indexación completada.\n");
}

Song readSongAt(FILE *file, long pos) {
    fseek(file, pos, SEEK_SET);
    char line[LINE_BUFFER];
    if (!fgets(line, sizeof(line), file)) {
        perror("Error leyendo línea de canción");
    }

    Song song = {0};
    char *tokens[12];
    char *token = strtok(line, ",");
    int i = 0;

    while (token && i < 12) {
        tokens[i++] = token;
        token = strtok(NULL, ",");
    }

    if (i >= 12) {
        snprintf(song.lastfm_url, sizeof(song.lastfm_url), "%s", tokens[0]);
        snprintf(song.track, sizeof(song.track), "%s", tokens[1]);
        snprintf(song.artist, sizeof(song.artist), "%s", tokens[2]);

        char *seed = strtok(tokens[3], "[]' ");
        while (seed && song.seed_count < MAX_SEEDS) {
            snprintf(song.seeds[song.seed_count++], MAX_FIELD, "%s", seed);
            seed = strtok(NULL, ",' ");
        }

        song.number_of_emotions = atof(tokens[4]);
        song.valence_tags = atof(tokens[5]);
        song.arousal_tags = atof(tokens[6]);
        song.dominance_tags = atof(tokens[7]);

        snprintf(song.genre, sizeof(song.genre), "%s", tokens[11]);
    }

    return song;
}

void printSong(Song s) {
    printf("\n🎶 === Canción encontrada ===\n");
    printf("🎵 Track: %s\n", s.track);
    printf("🎤 Artista: %s\n", s.artist);
    printf("📀 Género: %s\n", s.genre);
    printf("💬 Emociones: ");
    for (int i = 0; i < s.seed_count; i++) {
        printf("%s%s", s.seeds[i], (i < s.seed_count - 1) ? ", " : "");
    }
    printf("\n🎚️ Valence: %.2f | Arousal: %.2f | Dominance: %.2f\n", s.valence_tags, s.arousal_tags, s.dominance_tags);
    printf("🔗 URL: %s\n", s.lastfm_url);
}

void searcher(const char *filename) {
    mkfifo(PIPE_REQ, 0666);
    mkfifo(PIPE_RES, 0666);

    buildIndex(filename);

    int fd_req = open(PIPE_REQ, O_RDONLY);
    int fd_res = open(PIPE_RES, O_WRONLY);
    FILE *file = fopen(filename, "r");

    char keyword[MAX_FIELD];
    while (read(fd_req, keyword, MAX_FIELD) > 0) {
        keyword[strcspn(keyword, "\n")] = '\0';

        printf("[searcher] Buscando canciones con emoción: '%s'\n", keyword);

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        unsigned int idx = hash(keyword);
        HashNode *node = hashTable[idx].head;
        int found = 0;

        while (node) {
            Song s = readSongAt(file, node->position);
            for (int i = 0; i < s.seed_count; i++) {
                if (strcmp(s.seeds[i], keyword) == 0) {
                    ssize_t bytes_written = write(fd_res, &s, sizeof(Song));
                    if (bytes_written != sizeof(Song)) {
                        perror("Error al escribir canción");
                    }
                    found = 1;
                }
            }
            node = node->next;
        }

        if (!found) {
            Song empty = {0};
            if (write(fd_res, &empty, sizeof(Song)) != sizeof(Song)) {
                perror("Error al escribir vacío");
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_spent = (end.tv_sec - start.tv_sec) +
                            (end.tv_nsec - start.tv_nsec) / 1e9;
        printf("[searcher] Tiempo de búsqueda: %.4f segundos\n", time_spent);
    }

    close(fd_req);
    close(fd_res);
    fclose(file);
    clearHash();
}

void interface() {
    int fd_req = open(PIPE_REQ, O_WRONLY);
    int fd_res = open(PIPE_RES, O_RDONLY);

    while (1) {
        char input[MAX_FIELD];
        printf("\n💡 Bienvenido al Buscador Musical MuSe\n");
        printf("Ingrese una emoción para buscar (o 'salir'): ");
        if (!fgets(input, MAX_FIELD, stdin)) {
            perror("Error leyendo entrada");
            break;
        }
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "salir") == 0) break;

        if (write(fd_req, input, MAX_FIELD) != MAX_FIELD) {
            perror("Error al enviar búsqueda");
            continue;
        }

        while (1) {
            Song s;
            ssize_t bytes_read = read(fd_res, &s, sizeof(Song));
            if (bytes_read != sizeof(Song)) {
                perror("Error al leer canción");
                break;
            }
            if (strlen(s.track) == 0) break;
            printSong(s);
        }
    }

    close(fd_req);
    close(fd_res);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <modo> <archivo_csv>\n", argv[0]);
        printf("Modos disponibles: searcher | interface\n");
        return 1;
    }

    if (strcmp(argv[1], "searcher") == 0)
        searcher(argv[2]);
    else if (strcmp(argv[1], "interface") == 0)
        interface();
    else
        printf("Modo desconocido.\n");

    return 0;
}
