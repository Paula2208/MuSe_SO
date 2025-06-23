#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#define MAX_FIELD 256
#define MAX_SEEDS 10
#define HASH_SIZE 1000
#define LINE_BUFFER 2048
#define PIPE_REQ "/tmp/search_req"
#define PIPE_RES "/tmp/search_res"

volatile sig_atomic_t exit_requested = 0;

void handle_sigint(int sig) {
    printf("\n\n⏹️  Interrupción detectada (Ctrl+C). Cerrando programa con seguridad...\n");
    exit_requested = 1;
}

// Estructura de la canción
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

// Nodo de la tabla hash
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
        hash = (hash << 5) + tolower(*str++);
    return hash % HASH_SIZE;
}

// Elimina espacios y pone en minúscula
void sanitize_input(char *str) {
    // Eliminar espacios al inicio
    while (isspace(*str)) memmove(str, str + 1, strlen(str));

    // Eliminar espacios al final
    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) *end-- = '\0';

    // Convertir a minúsculas
    for (int i = 0; str[i]; i++) str[i] = tolower(str[i]);
}

void insertHash(const char *key, long pos) {
    char key_lower[MAX_FIELD];
    snprintf(key_lower, sizeof(key_lower), "%s", key);
    sanitize_input(key_lower);

    unsigned int idx = hash(key_lower);
    HashNode *newNode = malloc(sizeof(HashNode));
    if (!newNode) { perror("malloc"); exit(1); }
    newNode->position = pos;
    newNode->next = hashTable[idx].head;
    hashTable[idx].head = newNode;
    snprintf(hashTable[idx].key, sizeof(hashTable[idx].key), "%s", key_lower);
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
    if (!fgets(line, sizeof(line), file)) { // Saltar encabezado
        perror("Erro al saltar encabezado");
        exit(1);
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

        char artist_clean[MAX_FIELD] = "";
        if (i >= 3 && tokens[2]) {
            snprintf(artist_clean, sizeof(artist_clean), "%s", tokens[2]);
            insertHash(artist_clean, pos);
        }

        if (i >= 4 && tokens[3]) {
            char seeds_raw[MAX_FIELD];
            snprintf(seeds_raw, sizeof(seeds_raw), "%s", tokens[3]);
            char *seed = strtok(seeds_raw, "[]' ");
            while (seed) {
                insertHash(seed, pos);

                if (strlen(artist_clean) > 0) {
                    char combo[MAX_FIELD * 2];
                    snprintf(combo, sizeof(combo), "%s_%s", seed, artist_clean);
                    sanitize_input(combo);
                    insertHash(combo, pos);
                }
                seed = strtok(NULL, ",'");
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
        perror("Erro al obtener punto");
        exit(1);
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
            seed = strtok(NULL, ",'");
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
    printf("💼 Género: %s\n", s.genre);
    printf("💬 Emociones: ");
    for (int i = 0; i < s.seed_count; i++) {
        printf("%s%s", s.seeds[i], (i < s.seed_count - 1) ? ", " : "");
    }
    printf("\n🎚️ Valence: %.2f | Arousal: %.2f | Dominance: %.2f\n",
           s.valence_tags, s.arousal_tags, s.dominance_tags);
    printf("🔗 URL: %s\n", s.lastfm_url);
}


void mostrarMenuPrincipal() {
    printf("\n🌟 Menú Principal:\n");
    printf("1. Filtrar por emoción\n");
    printf("2. Filtrar por artista\n");
    printf("3. Filtrar por emoción y artista\n");
    printf("9. Salir\n");
    printf("Seleccione una opción: ");
}

void searcher(const char *filename) {
    mkfifo(PIPE_REQ, 0666);
    mkfifo(PIPE_RES, 0666);

    buildIndex(filename);

    int fd_req = open(PIPE_REQ, O_RDONLY);
    int fd_res = open(PIPE_RES, O_WRONLY);
    FILE *file = fopen(filename, "r");

    char keyword[MAX_FIELD];
    while (!exit_requested && read(fd_req, keyword, MAX_FIELD) > 0) {
        keyword[strcspn(keyword, "\n")] = '\0';
        sanitize_input(keyword);

        printf("[searcher] Buscando canciones con: '%s'\n", keyword);

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        unsigned int idx = hash(keyword);
        HashNode *node = hashTable[idx].head;

        while (node) {
            Song s = readSongAt(file, node->position);

            int match = 0;

            // Revisar combinación emoción + artista
            for (int i = 0; i < s.seed_count; i++) {
                char combined[MAX_FIELD * 2];
                snprintf(combined, sizeof(combined), "%s_%s", s.seeds[i], s.artist);
                sanitize_input(combined);
                if (strcmp(combined, keyword) == 0) {
                    match = 1;
                    break;
                }
            }

            // Si no fue match con combinación, revisar individuales
            if (!match) {
                for (int i = 0; i < s.seed_count; i++) {
                    char tmp[MAX_FIELD];
                    snprintf(tmp, MAX_FIELD, "%s", s.seeds[i]);
                    sanitize_input(tmp);
                    if (strcmp(tmp, keyword) == 0) match = 1;
                }

                char tmp[MAX_FIELD];
                snprintf(tmp, MAX_FIELD, "%s", s.artist);
                sanitize_input(tmp);
                if (strcmp(tmp, keyword) == 0) match = 1;

                snprintf(tmp, MAX_FIELD, "%s", s.track);
                sanitize_input(tmp);
                if (strcmp(tmp, keyword) == 0) match = 1;

                snprintf(tmp, MAX_FIELD, "id_%ld", node->position);
                sanitize_input(tmp);
                if (strcmp(tmp, keyword) == 0) match = 1;
            }

            if (match) {
                ssize_t bytes_written = write(fd_res, &s, sizeof(Song));
                if (bytes_written != sizeof(Song)) {
                    perror("Error al escribir canción");
                }
            }

            node = node->next;
        }

        Song terminator = {0};
        if (write(fd_res, &terminator, sizeof(Song)) != sizeof(Song)) {
            perror("Error al escribir vacío");
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

    printf("[searcher] Finalizando proceso correctamente. ❤️\n");
}

void interface() {
    int fd_req = open(PIPE_REQ, O_WRONLY);
    int fd_res = open(PIPE_RES, O_RDONLY);

    printf("\n>⩊< Bienvenido al buscador de canciones por sentimientos ▶︎ •၊၊||၊|။||||| 0:10\n");

    while (!exit_requested) {
        mostrarMenuPrincipal();

        char opcion[MAX_FIELD];
        if (!fgets(opcion, MAX_FIELD, stdin)) break;

        int op = atoi(opcion);
        if (op == 9) break;

        if (op >= 1 && op <= 3) {
            char input[MAX_FIELD] = "";
            if (op == 1) {
                printf("\nIngrese una emoción para buscar: ");
                if (!fgets(input, MAX_FIELD, stdin)) continue;
                input[strcspn(input, "\n")] = '\0';
                sanitize_input(input);
            }
            else if (op == 2) {
                printf("\nIngrese el nombre del artista: ");
                if (!fgets(input, MAX_FIELD, stdin)) continue;
                input[strcspn(input, "\n")] = '\0';
                sanitize_input(input);
            }
            else if (op == 3) {
                char emotion[MAX_FIELD], artist[MAX_FIELD];
                printf("\nIngrese la emoción: ");
                if (!fgets(emotion, MAX_FIELD, stdin)) continue;
                emotion[strcspn(emotion, "\n")] = '\0';
                sanitize_input(emotion);

                printf("Ingrese el artista: ");
                if (!fgets(artist, MAX_FIELD, stdin)) continue;
                artist[strcspn(artist, "\n")] = '\0';
                sanitize_input(artist);

                if (strlen(emotion) + strlen(artist) + 1 < MAX_FIELD) {
                    snprintf(input, MAX_FIELD, "%s_%s", emotion, artist);
                } else {
                    fprintf(stderr, "❌ Error: La combinación emoción + artista es demasiado larga.\n");
                    continue;
                }
            }

            if (write(fd_req, input, MAX_FIELD) != MAX_FIELD) {
                perror("Error al enviar búsqueda");
                continue;
            }

            while (1) {
                Song s;
                ssize_t bytes_read = read(fd_res, &s, sizeof(Song));
                if (bytes_read != sizeof(Song)) break;
                if (strlen(s.track) == 0) break;
                printSong(s);
            }

            printf("\n🔁 Volviendo al menú principal...\n");
        } else {
            printf("\n🚧 Esta opción aún no está implementada.\n");
        }
    }

    close(fd_req);
    close(fd_res);
    printf("\n¡Hasta pronto! ᡣ • . • 𐭩 ♡\n");
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

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
