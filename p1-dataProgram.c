#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>

#define MAX_FIELD 256
#define MAX_SEEDS 10
#define HASH_SIZE 1000
#define LINE_BUFFER 2048
#define PIPE_REQ "./output/search_req.pipe"
#define PIPE_RES "./output/search_res.pipe"
#define READY_FLAG "./output/searcher.ready"
#define INDEX_BIN_PATH "./output/index.bin"

volatile sig_atomic_t exit_requested = 0;

void clearHash();  

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


void print_memory_usage() {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) {
        perror("fopen");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            printf("📦 Memoria en uso (RAM): %s", line + 6); // Ya incluye unidades (kB)
            break;
        }
    }

    fclose(f);
}

// Guardar tabla hash a disco
void saveIndex(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("No se pudo guardar el índice");
        return;
    }

    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode *node = hashTable[i].head;
        while (node) {
            // Guardar clave del bucket + posición
            fwrite(&i, sizeof(int), 1, f);
            fwrite(hashTable[i].key, sizeof(char), MAX_FIELD, f);
            fwrite(&node->position, sizeof(long), 1, f);
            node = node->next;
        }
    }

    fclose(f);
    printf("[indexer] Índice guardado exitosamente en %s\n", path);
}

// Cargar tabla hash desde disco
void loadIndex(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("No se pudo cargar el índice");
        return;
    }

    clearHash(); // Por si hubiera residuos

    int bucket_index;
    char key[MAX_FIELD];
    long pos;

    while (fread(&bucket_index, sizeof(int), 1, f) == 1 &&
           fread(key, sizeof(char), MAX_FIELD, f) == MAX_FIELD &&
           fread(&pos, sizeof(long), 1, f) == 1) {

        HashNode *newNode = malloc(sizeof(HashNode));
        if (!newNode) { perror("malloc"); exit(1); }
        newNode->position = pos;
        newNode->next = hashTable[bucket_index].head;
        hashTable[bucket_index].head = newNode;
        snprintf(hashTable[bucket_index].key, MAX_FIELD, "%s", key);
    }

    fclose(f);
    printf("[indexer] Índice cargado desde %s\n", path);
}

// Elimina espacios y pone en minúscula
void sanitize_input(char *str) {
    // Eliminar espacios al inicio
    char *start = str;
    while (isspace((unsigned char)*start)) start++;
    memmove(str, start, strlen(start) + 1);

    // Eliminar espacios al final
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) *end-- = '\0';

    // Convertir a minúsculas
    for (int i = 0; str[i]; i++) str[i] = tolower((unsigned char)str[i]);
}

void insertHash(const char *key, long pos) {
    char key_lower[MAX_FIELD];
    snprintf(key_lower, sizeof(key_lower), "%s", key);
    sanitize_input(key_lower);

    if (strlen(key_lower) == 0) return; // No insertar llaves vacías

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
    
    // Saltar encabezado
    if (!fgets(line, sizeof(line), file)) {
        perror("Error al saltar encabezado");
        fclose(file);
        exit(1);
    }
    long pos = ftell(file); // <-- CAMBIO: Obtener posición después de leer el encabezado

    printf("[indexer] Indexando el archivo CSV...\n");
    
    long line_count = 0;
    while (fgets(line, sizeof(line), file)) {
        line_count++;
        
        if (line_count % 700000 == 0) {
            printf("[indexer] Procesadas %ld líneas...\n", line_count);
        }
        
        char *tokens[12];
        char *p_line = line;

        for (int i=0; i < 12; i++) {
            char *start = p_line;
            while(*p_line && *p_line != ',') p_line++;
            if (*p_line) {
                *p_line = '\0';
                p_line++;
            }
            tokens[i] = start;
        }

        char artist_clean[MAX_FIELD] = "";
        if (tokens[2]) {
            snprintf(artist_clean, sizeof(artist_clean), "%s", tokens[2]);
            insertHash(artist_clean, pos);
        }

        if (tokens[3]) {
            char seeds_raw[MAX_FIELD];
            snprintf(seeds_raw, sizeof(seeds_raw), "%s", tokens[3]);
            char *seed = strtok(seeds_raw, "[]' ");
            while (seed) {
                if (strlen(seed) > 0) {
                    insertHash(seed, pos);
                    if (strlen(artist_clean) > 0) {
                        char combo[MAX_FIELD * 2];
                        snprintf(combo, sizeof(combo), "%s_%s", seed, artist_clean);
                        insertHash(combo, pos);
                    }
                }
                seed = strtok(NULL, ",' ");
            }
        }

        pos = ftell(file);
    }

    fclose(file);
    printf("[indexer] Indexación completada. Total de líneas procesadas: %ld\n", line_count);
}

Song readSongAt(FILE *file, long pos) {
    Song song = {0};
    if (fseek(file, pos, SEEK_SET) != 0) {
        perror("Error al posicionar archivo");
        return song;
    }
    
    char line[LINE_BUFFER];
    if (!fgets(line, sizeof(line), file)) {
        // No es un error fatal, podría ser EOF
        return song;
    }

    // Usar strsep para manejar mejor los campos vacíos
    char *p_line = line;
    char *token;
    int i = 0;

    // Asumimos 12 campos como en la indexación
    char *tokens[12] = {0};
    while ((token = strsep(&p_line, ",")) != NULL && i < 12) {
        tokens[i++] = token;
    }
    
    if (i >= 12) {
        snprintf(song.lastfm_url, sizeof(song.lastfm_url), "%s", tokens[0] ? tokens[0] : "");
        snprintf(song.track, sizeof(song.track), "%s", tokens[1] ? tokens[1] : "");
        snprintf(song.artist, sizeof(song.artist), "%s", tokens[2] ? tokens[2] : "");

        if (tokens[3]) {
            char seeds_copy[MAX_FIELD];
            snprintf(seeds_copy, sizeof(seeds_copy), "%s", tokens[3]);
            char *seed = strtok(seeds_copy, "[]' ");
            while (seed && song.seed_count < MAX_SEEDS) {
                if(strlen(seed) > 0) {
                    snprintf(song.seeds[song.seed_count++], MAX_FIELD, "%s", seed);
                }
                seed = strtok(NULL, ",' ");
            }
        }

        song.number_of_emotions = tokens[4] ? atof(tokens[4]) : 0.0;
        song.valence_tags = tokens[5] ? atof(tokens[5]) : 0.0;
        song.arousal_tags = tokens[6] ? atof(tokens[6]) : 0.0;
        song.dominance_tags = tokens[7] ? atof(tokens[7]) : 0.0;
        snprintf(song.genre, sizeof(song.genre), "%s", tokens[11] ? tokens[11] : "");
        // Limpiar saltos de línea del último campo
        song.genre[strcspn(song.genre, "\r\n")] = 0;
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

void wait_for_ready_signal() {
    printf("[interface] Esperando que el searcher esté listo...\n");
    while (!exit_requested) {
        if (access(READY_FLAG, F_OK) == 0) { // <-- CAMBIO: access() es más simple que stat() para solo chequear existencia
            printf("[interface] Searcher está listo.\n");
            break;
        }
        usleep(100000); // Esperar 100ms
    }
}

void create_ready_signal() {
    FILE *ready_file = fopen(READY_FLAG, "w");
    if (ready_file) {
        fclose(ready_file);
        printf("[searcher] Señal de listo creada.\n");
    } else {
        perror("Error creando archivo de señal");
    }
}

void searcher(const char *filename) {

    if (access(INDEX_BIN_PATH, F_OK) == 0) {
        printf("[searcher] Archivo binario de índice encontrado. Cargando...\n");
        loadIndex(INDEX_BIN_PATH);
    } else {
        printf("[searcher] No hay índice guardado. Indexando desde CSV...\n");
        buildIndex(filename);

        pid_t pid = fork();
        if (pid == 0) {
            saveIndex(INDEX_BIN_PATH);
            printf("[searcher] Proceso hijo terminó de guardar el índice.\n");
            _exit(0);
        } else if (pid < 0) {
            perror("[searcher] Error al crear proceso para guardar el índice");
        } else {
            printf("[searcher] Proceso hijo creado para guardar índice (PID: %d)\n", pid);
        }
    }

    print_memory_usage();

    create_ready_signal();

    int fd_req = open(PIPE_REQ, O_RDONLY);
    if (fd_req == -1) {
        perror("[searcher] ERROR abriendo PIPE_REQ");
        exit(1);
    }

    int fd_res = open(PIPE_RES, O_WRONLY);
    if (fd_res == -1) {
        perror("[searcher] ERROR abriendo PIPE_RES");
        close(fd_req);
        exit(1);
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo archivo para búsqueda");
        close(fd_req);
        close(fd_res);
        exit(1);
    }

    char keyword[MAX_FIELD];
    while (!exit_requested) {
        printf("[searcher] Esperando una nueva búsqueda...\n");
        ssize_t bytes_read = read(fd_req, keyword, MAX_FIELD);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("[searcher] Conexión cerrada por la interfaz.\n");
            } else if (errno != EINTR) {
                perror("[searcher] Error leyendo del pipe");
            }
            break;
        }

        keyword[bytes_read] = '\0';
        sanitize_input(keyword);
        if (strlen(keyword) == 0) continue;

        printf("[searcher] Buscando canciones con: '%s'\n", keyword);

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        unsigned int idx = hash(keyword);
        HashNode *node = hashTable[idx].head;

        int capacity = 1000;
        int match_count = 0;
        Song *matches = malloc(sizeof(Song) * capacity);
        if (!matches) {
            perror("malloc");
            break;
        }

        while (node && !exit_requested) {
            Song s = readSongAt(file, node->position);

            int match = 0;
            char temp_key[MAX_FIELD];

            snprintf(temp_key, sizeof(temp_key), "%s", s.artist);
            sanitize_input(temp_key);
            if (strcmp(temp_key, keyword) == 0) {
                match = 1;
            }

            if (!match) {
                for (int i = 0; i < s.seed_count; i++) {
                    snprintf(temp_key, sizeof(temp_key), "%s", s.seeds[i]);
                    sanitize_input(temp_key);
                    if (strcmp(temp_key, keyword) == 0) {
                        match = 1;
                        break;
                    }
                }
            }

            if (!match) {
                for (int i = 0; i < s.seed_count; i++) {
                    char artist_sanitized[MAX_FIELD];
                    char seed_sanitized[MAX_FIELD];

                    snprintf(artist_sanitized, sizeof(artist_sanitized), "%s", s.artist);
                    sanitize_input(artist_sanitized);

                    snprintf(seed_sanitized, sizeof(seed_sanitized), "%s", s.seeds[i]);
                    sanitize_input(seed_sanitized);

                    char combined[MAX_FIELD * 2];
                    snprintf(combined, sizeof(combined), "%s_%s", seed_sanitized, artist_sanitized);

                    if (strcmp(combined, keyword) == 0) {
                        match = 1;
                        break;
                    }
                }
            }

            if (match) {
                if (match_count == capacity) {
                    capacity *= 2;
                    Song *tmp = realloc(matches, sizeof(Song) * capacity);
                    if (!tmp) {
                        perror("realloc");
                        free(matches);
                        break;
                    }
                    matches = tmp;
                }
                matches[match_count++] = s;
            }

            node = node->next;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_spent = (end.tv_sec - start.tv_sec) +
                            (end.tv_nsec - start.tv_nsec) / 1e9;

        printf("[searcher] Búsqueda completada: %d canciones encontradas en %.3f segundos\n", 
               match_count, time_spent);

        // Envia banderita de lo encontrado
        int total_found = match_count;
        if (write(fd_res, &total_found, sizeof(int)) != sizeof(int)) {
            if (errno != EPIPE) perror("Error al enviar total de canciones");
            continue;
        }

        // Esperar confirmación del interface (char de 1 byte: 'y' o 'n')
        char respuesta_usuario;
        ssize_t r = read(fd_req, &respuesta_usuario, 1);
        if (r <= 0 || respuesta_usuario != 'y') {
            continue; // No se envían las canciones
        }

        // Enviar resultados (fuera del tiempo medido)
        for (int i = 0; i < match_count; i++) {
            if (write(fd_res, &matches[i], sizeof(Song)) != sizeof(Song)) {
                if (errno != EPIPE) perror("Error al escribir canción");
                break;
            }
        }

        Song terminator = {0};
        if (write(fd_res, &terminator, sizeof(Song)) != sizeof(Song)) {
            if (errno != EPIPE) perror("Error al escribir terminador");
        }

        free(matches);
    }

    close(fd_req);
    close(fd_res);
    fclose(file);
    clearHash();

    printf("[searcher] Finalizando proceso correctamente. ❤️\n");
}

void interface() {
    wait_for_ready_signal();
    if (exit_requested) return;

    int fd_req, fd_res;

    printf("[interface] Abriendo FIFO de escritura %s...\n", PIPE_REQ);
    fd_req = open(PIPE_REQ, O_WRONLY);
    if (fd_req == -1) {
        printf("[interface] ERROR: open PIPE_REQ: %s\n", strerror(errno));
        exit(1);
    }
    printf("[interface] FIFO de escritura abierto.\n");

    printf("[interface] Abriendo FIFO de lectura %s...\n", PIPE_RES);
    fd_res = open(PIPE_RES, O_RDONLY);
    if (fd_res == -1) {
        printf("[interface] ERROR: open PIPE_RES: %s\n", strerror(errno));
        close(fd_req);
        exit(1);
    }
    printf("[interface] FIFO de lectura abierto.\n");

    printf("\n   >⩊< Bienvenido al buscador de canciones por sentimientos ▶︎ •၊၊||၊|။||||| 0:10       \n");

    while (!exit_requested) {
        mostrarMenuPrincipal();

        char choice_str[10];
        if (!fgets(choice_str, sizeof(choice_str), stdin)) break;

        int op = atoi(choice_str);
        if (op == 9) break;

        if (op >= 1 && op <= 3) {
            char input[MAX_FIELD * 2] = "";
            char buffer[MAX_FIELD] = "";

            if (op == 1) {
                printf("\nIngrese una emoción para buscar: ");
                if (!fgets(buffer, sizeof(buffer), stdin)) continue;
                snprintf(input, sizeof(input), "%s", buffer);
            }
            else if (op == 2) {
                printf("\nIngrese el nombre del artista: ");
                if (!fgets(buffer, sizeof(buffer), stdin)) continue;
                snprintf(input, sizeof(input), "%s", buffer);
            }
            else if (op == 3) {
                char emotion[MAX_FIELD], artist[MAX_FIELD];
                printf("\nIngrese la emoción: ");
                if (!fgets(emotion, sizeof(emotion), stdin)) continue;
                emotion[strcspn(emotion, "\n")] = '\0';
                sanitize_input(emotion);

                printf("Ingrese el artista: ");
                if (!fgets(artist, sizeof(artist), stdin)) continue;
                artist[strcspn(artist, "\n")] = '\0';
                sanitize_input(artist);

                snprintf(input, sizeof(input), "%s_%s", emotion, artist);
            }

            sanitize_input(input);

            if (strlen(input) == 0) {
                printf("❌ Error: Entrada vacía.\n");
                continue;
            }

            printf("\n🔍 Buscando: '%s'...\n", input);

            if (write(fd_req, input, strlen(input)) == -1) {
                 if (errno != EPIPE) perror("Error al enviar búsqueda");
                break; // El searcher probablemente ha muerto
            }

            // Esperar la cantidad de canciones encontradas
            int total_encontradas = 0;
            if (read(fd_res, &total_encontradas, sizeof(int)) != sizeof(int)) {
                perror("Error leyendo cantidad de resultados");
                exit_requested = 1; 
                continue;
            }

            if (total_encontradas == 0) {
                printf("\n❌ No se encontraron canciones con ese criterio.\n");
                continue;
            }

            // Preguntar si quiere verlas
            char respuesta[4];
            printf("\n🎵 Se encontraron %d canciones. ¿Desea mostrarlas? (s/n): ", total_encontradas);
            if (!fgets(respuesta, sizeof(respuesta), stdin)) {
                continue;
            }
            sanitize_input(respuesta);

            char confirm = (respuesta[0] == 's' || respuesta[0] == 'S') ? 'y' : 'n';
            // Enviar confirmación al searcher
            if (write(fd_req, &confirm, 1) != 1) {
                perror("Error enviando confirmación al searcher");
                exit_requested = 1; 
                break;
            }

            if (confirm == 'n') {
                printf("📭 Resultados omitidos. Volviendo al menú...\n");
                continue;
            }

            int song_count = 0;
            while (!exit_requested) {
                Song s;
                ssize_t bytes_read = read(fd_res, &s, sizeof(Song));
                if (bytes_read == 0) {
                    printf("[interface] El searcher cerró la conexión.\n");
                    exit_requested = 1; // Salir del bucle principal
                    break;
                }
                if (bytes_read < sizeof(Song)) {
                    perror("Error leyendo respuesta");
                    break;
                }
                if (strlen(s.track) == 0) break; // Terminador
                
                song_count++;
                printSong(s);
            }

            if (song_count == 0) {
                printf("\n❌ No se encontraron canciones con ese criterio.\n");
            } else {
                printf("\n✅ Total de canciones encontradas: %d\n", song_count);
            }

            printf("\n🔁 Volviendo al menú principal...\n");
        } else {
            printf("\n🚧 Opción no válida. Intente nuevamente.\n");
        }
    }

    close(fd_req);
    close(fd_res);
    printf("\n¡Hasta pronto! ᡣ • . • 𐭩 ♡\n");
}

void setup_environment() {
    printf("[main] Limpiando entorno anterior y creando FIFOs...\n");
    
    // Eliminar FIFOs y flag existentes para empezar limpio
    unlink(PIPE_REQ);
    unlink(PIPE_RES);
    unlink(READY_FLAG);
    
    if (mkfifo(PIPE_REQ, 0666) == -1) {
        if (errno != EEXIST) {
            perror("[main] Error creando PIPE_REQ");
            exit(1);
        }
    }
    printf("[main] FIFO %s creado.\n", PIPE_REQ);

    if (mkfifo(PIPE_RES, 0666) == -1) {
        if (errno != EEXIST) {
            perror("[main] Error creando PIPE_RES");
            unlink(PIPE_REQ);
            exit(1);
        }
    }
    printf("[main] FIFO %s creado.\n", PIPE_RES);
}


int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    if (argc < 2) { // <-- CAMBIO: Permitir que la interfaz no necesite el CSV
        fprintf(stderr, "Uso: %s <modo> [archivo_csv]\n", argv[0]);
        fprintf(stderr, "Modos disponibles: searcher | interface\n");
        return 1;
    }

    if (strcmp(argv[1], "searcher") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Uso: %s searcher <archivo_csv>\n", argv[0]);
            return 1;
        }
        printf("[main] Ejecutando Searcher\n");
        setup_environment();
        searcher(argv[2]);
    }
    else if (strcmp(argv[1], "interface") == 0) {
        printf("[main] Ejecutando Interface\n");
        interface();
    }
    else {
        fprintf(stderr, "Modo desconocido: %s\n", argv[1]);
        return 1;
    }

    return 0;
}