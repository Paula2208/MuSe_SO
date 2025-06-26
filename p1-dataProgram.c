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
#define MAX_KEY MAX_FIELD * 3 // Para la clave "arousal_emotion_artist"
#define NUM_FIELDS 12 // Número de campos esperados en el CSV
#define MAX_SEEDS 20
#define HASH_SIZE 1000
#define LINE_BUFFER 2048
#define PIPE_REQ "./output/search_req.pipe"
#define PIPE_RES "./output/search_res.pipe"
#define READY_FLAG "./output/searcher.ready"
#define INDEX_BIN_PATH "./output/index.bin"

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
    char key[MAX_KEY];
    HashNode* head;
} HashBucket;

HashBucket hashTable[HASH_SIZE];

volatile sig_atomic_t exit_requested = 0;

void handle_sigint(int sig) {
    printf("\n\n⏹️  Interrupción detectada (Ctrl+C). Cerrando programa con seguridad...\n");
    exit_requested = 1;
}

/**
 * Calcula el valor hash de una cadena de caracteres usando una función hash simple.
 * 
 * @param str Cadena de entrada (clave) a hashear. Puede contener mayúsculas o minúsculas.
 * @return Un valor entero que representa el índice en la tabla hash, entre 0 y HASH_SIZE - 1.
 */
unsigned int hash(const char *str) {
    unsigned int hash = 0;
    while (*str)
        hash = (hash << 5) + tolower(*str++);
    return hash % HASH_SIZE;
}

/**
 * Libera toda la memoria ocupada por la tabla hash.
 * 
 * Recorre cada bucket de la tabla hash y libera los nodos enlazados,
 * dejando los punteros en NULL. Esta función se usa antes de recargar
 * un índice o al finalizar la ejecución para evitar fugas de memoria.
 */
void clearHash() {
    for (int i = 0; i < HASH_SIZE; i++) {
        HashNode *curr = hashTable[i].head;
        while (curr) {
            HashNode *temp = curr;
            curr = curr->next;
            free(temp); // Liberar cada nodo
        }
        hashTable[i].head = NULL; // Marcar bucket como vacío
    }
}

/**
 * Guarda el contenido de la tabla hash en un archivo binario.
 * 
 * @param path Ruta del archivo donde se almacenará el índice serializado.
 *             El archivo se escribe en modo binario ("wb").
 * 
 * Cada entrada guardada contiene:
 *   - Índice del bucket (int)
 *   - Clave asociada al bucket (char[MAX_FIELD])
 *   - Posición del nodo (long)
 */
void saveIndex(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("No se pudo guardar el índice");
        return;
    }

    // Recorrer todos los buckets de la tabla hash
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

/**
 * Carga el índice previamente guardado desde un archivo binario.
 *
 * @param path Ruta al archivo binario que contiene el índice serializado.
 * 
 * Esta función reconstruye la tabla hash en memoria a partir de los datos
 * almacenados en disco. Cada entrada incluye:
 *   - Índice del bucket
 *   - Clave asociada al bucket
 *   - Posición del nodo (offset en el archivo CSV)
 * 
 * Si el archivo no puede abrirse, se muestra un error.
 */
void loadIndex(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("No se pudo cargar el índice");
        return;
    }

    clearHash(); // Limpiar posibles datos previos en la tabla

    int bucket_index;
    char key[MAX_FIELD];
    long pos;

    while (fread(&bucket_index, sizeof(int), 1, f) == 1 &&
           fread(key, sizeof(char), MAX_FIELD, f) == MAX_FIELD &&
           fread(&pos, sizeof(long), 1, f) == 1) {

        // Crear un nuevo nodo para la entrada
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

/**
 * Inserta una nueva entrada en la tabla hash.
 *
 * @param key Clave con formato 'arousal_emotion_artist', ya sanitizada.
 * @param pos Posición en el archivo donde se encuentra la canción.
 *
 * La función calcula el índice hash correspondiente a la clave y
 * agrega un nuevo nodo al bucket correspondiente. Si ya hay nodos en
 * el bucket, el nuevo se agrega al inicio de la lista enlazada.
 */
void insertHash(const char *key, long pos) {
    char key_lower[MAX_KEY];
    snprintf(key_lower, sizeof(key_lower), "%s", key);

    if (strlen(key_lower) == 0) return;  // No insertar llaves vacías

    unsigned int idx = hash(key_lower);  // Cálculo del índice del bucket en la tabla hash
    HashNode *newNode = malloc(sizeof(HashNode));  // Crear un nuevo nodo
    if (!newNode) { perror("malloc"); exit(1); }
    newNode->position = pos;
    newNode->next = hashTable[idx].head;  // Insertar al principio de la lista enlazada
    hashTable[idx].head = newNode;
    snprintf(hashTable[idx].key, sizeof(hashTable[idx].key), "%s", key_lower);  // Almacenar la clave
}

/**
 * Limpia una cadena eliminando cualquier carácter que no sea una letra
 * y convierte todos los caracteres a minúsculas.
 *
 * @param str Cadena a sanitizar. La transformación es in-place (se modifica directamente).
 *
 * Esta función se usa para normalizar entradas como emociones y nombres de artistas
 * eliminando espacios, signos de puntuación y otros caracteres no alfabéticos.
 */
void sanitize_input(char *str) {
    // char original[MAX_FIELD];
    // snprintf(original, sizeof(original), "%s", str);  // Guardar copia de str original

    char *src = str, *dst = str;

    while (*src) {
        if (isalpha((unsigned char)*src)) {
            *dst = tolower((unsigned char)*src);
            dst++;
        }
        src++;
    }
    *dst = '\0';

    // printf("[indexer] Clave original: '%s' -> sanitizada: '%s'\n", original, str);
}

/**
 * Construye la tabla hash de índice a partir de un archivo CSV.
 *
 * @param filename Ruta al archivo CSV que contiene los datos de las canciones.
 *
 * Esta función:
 * - Abre el archivo CSV y salta el encabezado.
 * - Procesa cada línea para extraer los campos relevantes.
 * - Limpia y normaliza las emociones y el nombre del artista.
 * - Crea una clave con el formato: arousal_emotion_artist.
 * - Inserta cada clave en la tabla hash junto con la posición del archivo.
 * 
 * Solo se crean entradas por cada emoción individual presente en la canción.
 */
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
    long pos = ftell(file); // Obtener posición después de leer el encabezado

    printf("[indexer] Indexando el archivo CSV...\n");

    long line_count = 0;
    while (fgets(line, sizeof(line), file)) {
        line_count++;

        if (line_count % 700000 == 0) {
            printf("[indexer] Procesadas %ld líneas...\n", line_count);
        }

        char *tokens[NUM_FIELDS];
        char *p_line = line;

        
        // Separar línea por comas, tratando correctamente campos con corchetes
        for (int i = 0; i < NUM_FIELDS; i++) {
            char *start = p_line;

            while (*p_line && *p_line != ','){
                if(*p_line == '['){ // Lista de emociones
                    while (*p_line && *p_line != ']') p_line++;
                }
                p_line++;
            }
            

            if (*p_line) {
                *p_line = '\0';
                p_line++;
            }

            tokens[i] = start;
        }

        // Limpiar y extraer datos
        char artist_clean[MAX_FIELD] = "";
        if (tokens[2]) {
            snprintf(artist_clean, sizeof(artist_clean), "%s", tokens[2]);
            sanitize_input(artist_clean);  // Sanitizar el artista
        }

        // Procesar las emociones y crear claves solo si hay emoción
        if (tokens[3]) {
            char seeds_raw[MAX_FIELD];
            snprintf(seeds_raw, sizeof(seeds_raw), "%s", tokens[3]);
            char *seed = strtok(seeds_raw, ",");

            while (seed) {
                if (strlen(seed) > 0) {
                    sanitize_input(seed); // Limpiar la emoción
                    
                    if (strlen(seed) > 0 && strlen(artist_clean) > 0) {
                        int arousal = (int) atof(tokens[6]); // Solo la parte entera de arousal
                        char combo[MAX_KEY];
                        snprintf(combo, sizeof(combo), "%d_%s_%s", arousal, seed, artist_clean);
                        // printf("[indexer] Insertando clave: '%s' en posición %ld\n", combo, pos);
                        insertHash(combo, pos);  // Insertar la clave en el hash
                    }
                }
                seed = strtok(NULL, ",");
            }
        }

        pos = ftell(file); // Actualizar posición para la siguiente entrada
    }

    fclose(file);
    printf("[indexer] Indexación completada. Total de líneas procesadas: %ld\n", line_count);
}

/**
 * Lee y parsea una canción desde una posición específica en un archivo CSV.
 *
 * @param file Puntero al archivo CSV ya abierto.
 * @param pos Posición en el archivo desde la cual comenzar a leer.
 * @return Estructura Song con los datos extraídos desde la línea correspondiente.
 *
 * Esta función:
 * - Se posiciona en `pos` dentro del archivo.
 * - Lee una línea y la divide en campos considerando listas y comas.
 * - Extrae los datos relevantes, incluyendo una lista de emociones.
 */
Song readSongAt(FILE *file, long pos) {
    Song song = {0};
    if (fseek(file, pos, SEEK_SET) != 0) {
        perror("Error al posicionar archivo");
        return song;
    }
    
    char line[LINE_BUFFER];
    if (!fgets(line, sizeof(line), file)) {
        return song;
    }

    // Misma lógica de buildIndex para el parseo
    char *p_line = line;
    char *tokens[NUM_FIELDS] = {0};
    for (int i = 0; i < NUM_FIELDS; i++) {
        char *start = p_line;

        while (*p_line && *p_line != ','){
            if(*p_line == '['){
                while (*p_line && *p_line != ']') p_line++;
            }
            if (*p_line) p_line++;
        }
        
        if (*p_line) {
            *p_line = '\0';
            p_line++;
        }
        tokens[i] = start;
    }
    
    // Asignar los campos a la estructura Song
    if (tokens[0]) snprintf(song.lastfm_url, sizeof(song.lastfm_url), "%s", tokens[0]);
    if (tokens[1]) snprintf(song.track, sizeof(song.track), "%s", tokens[1]);
    if (tokens[2]) snprintf(song.artist, sizeof(song.artist), "%s", tokens[2]);

    if (tokens[3]) {
        char *p_seeds = tokens[3];
        while ((p_seeds = strchr(p_seeds, '\'')) != NULL && song.seed_count < MAX_SEEDS) {
            p_seeds++; // Mover el puntero más allá de la comilla de apertura
            char *end_quote = strchr(p_seeds, '\'');
            if (!end_quote) break; // Si no hay comilla de cierre, la línea está mal formada

            // Guardamos el carácter original y ponemos un terminador nulo para copiar
            char original_char = *end_quote;
            *end_quote = '\0';

            // Copiamos la emoción encontrada
            snprintf(song.seeds[song.seed_count++], MAX_FIELD, "%s", p_seeds);
            
            // Restauramos el carácter original para no alterar el resto del parseo
            *end_quote = original_char;

            // Avanzamos el puntero para la siguiente búsqueda
            p_seeds = end_quote + 1;
        }
    }

    if (tokens[4]) song.number_of_emotions = atof(tokens[4]);
    if (tokens[5]) song.valence_tags = atof(tokens[5]);
    if (tokens[6]) song.arousal_tags = atof(tokens[6]);
    if (tokens[7]) song.dominance_tags = atof(tokens[7]);
    
    // El token de género es el último, puede contener el salto de línea
    if (tokens[11]) {
        snprintf(song.genre, sizeof(song.genre), "%s", tokens[11]);
        song.genre[strcspn(song.genre, "\r\n")] = 0;
    }

    return song;
}

/**
 * Imprime los detalles de una canción en un formato legible.
 *
 * @param s Estructura Song con los datos de la canción a imprimir.
 *
 * Esta función muestra el track, artista, género, emociones y tags de valencia,
 * excitación y dominancia. También imprime la URL de Last.fm asociada.
 */
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

/**
 * Espera a que el searcher esté listo, verificando la existencia de un archivo de señal.
 *
 * Esta función se usa para sincronizar la interfaz con el proceso searcher,
 * asegurando que este último haya terminado de cargar o construir el índice
 * antes de que la interfaz comience a enviar solicitudes.
 */
void wait_for_ready_signal() {
    printf("[interface] Esperando que el searcher esté listo...\n");
    while (!exit_requested) {
        if (access(READY_FLAG, F_OK) == 0) { // <-- Access() es más simple que stat() para solo chequear existencia
            printf("[interface] Searcher está listo.\n");
            break;
        }
        usleep(100000); // Esperar 100ms
    }
}

/**
 * Imprime información de depuración sobre los buckets de la tabla hash.
 *
 * Esta función recorre cada bucket y muestra:
 * - La clave original del bucket.
 * - Cada nodo en el bucket, su posición y la clave asociada.
 * - El hash calculado para la clave del bucket.
 */
void debug_print_buckets() {
    for (int i = 0; i < 2; i++) {
        if (hashTable[i].head != NULL) {
            printf("\n[debug] Bucket %d:\n", i);
            printf("  Clave original en el bucket: %s\n", hashTable[i].key);
            
            // Imprimir cada nodo del bucket
            HashNode *node = hashTable[i].head;
            while (node) {
                printf("    Nodo: posición %ld\n", node->position);
                printf("    Clave del nodo: %s\n", hashTable[i].key);
                
                // Imprimir el hash calculado
                unsigned int bucket_hash = hash(hashTable[i].key);
                printf("    Hash calculado: %u\n", bucket_hash);
                
                node = node->next;
            }
        } else {
            printf("[debug] Bucket %d está vacío.\n", i);
        }
    }
}

/**
 * Crea un archivo de señal que indica que el proceso de búsqueda está listo.
 *
 * Este archivo (definido por la constante READY_FLAG) es usado para indicar 
 * al proceso interfaz que el indexador ha terminado de cargar o construir el índice.
 * No recibe parámetros ni retorna valor.
 */
void create_ready_signal() {
    FILE *ready_file = fopen(READY_FLAG, "w");
    if (ready_file) {
        fclose(ready_file);
        printf("[searcher] Señal de listo creada.\n");
    } else {
        perror("Error creando archivo de señal");
    }
}

/**
 * Crea el archivo de señal y maneja la lógica de búsqueda.
 *
 * @param filename Ruta al archivo CSV que contiene los datos de las canciones.
 *
 * Esta función:
 * - Verifica si ya existe un índice guardado.
 * - Si no, construye el índice desde el CSV y lo guarda en un proceso hijo.
 * - Abre los pipes para recibir solicitudes de búsqueda y enviar resultados.
 * - Procesa las búsquedas hasta que se solicite una interrupción.
 */
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

    // debug_print_buckets();

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

    char keyword[MAX_KEY];
    while (!exit_requested) {
        printf("\n\n[searcher] Esperando una nueva búsqueda...\n");
        ssize_t bytes_read = read(fd_req, keyword, MAX_KEY);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("[searcher] Conexión cerrada por la interfaz.\n");
            } else if (errno != EINTR) {
                perror("[searcher] Error leyendo del pipe");
            }
            break;
        }

        keyword[bytes_read] = '\0';

        if (strlen(keyword) == 0) continue;

        printf("[searcher] Buscando canciones con: '%s' Hash: %d\n", keyword, hash(keyword));

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
            char temp_key[MAX_KEY];

            sanitize_input(s.artist);

            int i = 0;
            while (i < MAX_SEEDS && strlen(s.seeds[i]) > 0) {
                char emotion_clean[MAX_FIELD];
                snprintf(emotion_clean, sizeof(emotion_clean), "%s", s.seeds[i]);
                sanitize_input(emotion_clean); // Limpiar cada emoción

                // Comparar la clave "arousal_emotion_artist"
                snprintf(temp_key, sizeof(temp_key), "%d_%s_%s", (int)s.arousal_tags, emotion_clean, s.artist);

                // printf("[searcher] Comparando con clave: '%s' input: '%s'\n", temp_key, keyword);

                if (strcmp(temp_key, keyword) == 0) {
                    match = 1;
                    break;
                }

                i++;
            }

            if (strcmp(temp_key, keyword) == 0) {
                match = 1;
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
               
        if (!matches) continue;

        // Enviar la cantidad de canciones encontradas
        int total_found = match_count;
        if (write(fd_res, &total_found, sizeof(int)) != sizeof(int)) {
            if (errno != EPIPE) perror("Error al enviar total de canciones");
            continue;
        }
        
        // Solo esperamos la confirmación Y enviamos resultados SI ENCONTRAMOS ALGO.
        if (total_found > 0) {
            char respuesta_usuario;
            ssize_t r = read(fd_req, &respuesta_usuario, 1);
            if (r > 0 && respuesta_usuario == 'y') {
                // Enviar resultados
                for (int i = 0; i < match_count; i++) {
                    if (write(fd_res, &matches[i], sizeof(Song)) != sizeof(Song)) {
                        if (errno != EPIPE) perror("Error al escribir canción");
                        break;
                    }
                }
                // Enviar terminador
                Song terminator = {0};
                if (write(fd_res, &terminator, sizeof(Song)) != sizeof(Song)) {
                    if (errno != EPIPE) perror("Error al escribir terminador");
                }
            }
        }

        free(matches);
    }

    close(fd_req);
    close(fd_res);
    fclose(file);
    clearHash();

    printf("[searcher] Finalizando proceso correctamente.\n");
    printf("\n¡Hasta pronto! ᡣ • . • 𐭩 ♡\n");
}

/**
 * Muestra el menú principal de la interfaz de usuario por consola.
 *
 * Esta función imprime las opciones disponibles para que el usuario 
 * interactúe con el sistema de búsqueda de canciones.
 * No recibe parámetros ni retorna valor.
 */
void mostrarMenuPrincipal() {
    printf("\n\n====================\n");
    printf("🌟 Menú Principal:\n");
    printf("1. Ingresar la intensidad de la emoción (0 a 100)\n");
    printf("2. Ingresar emoción\n");
    printf("3. Ingresar el artista\n");
    printf("4. Realizar la búsqueda\n");
    printf("9. Salir\n");
    printf("Seleccione una opción: ");
}

/**
 * Función principal de la interfaz de usuario.
 *
 * Se conecta a los FIFOs de comunicación con el proceso `searcher` y permite al usuario:
 * - Ingresar parámetros de búsqueda (arousal, emoción, artista).
 * - Enviar la búsqueda al `searcher` a través del pipe.
 * - Recibir el número de resultados y, si el usuario acepta, mostrarlos.
 * 
 * La función termina si el usuario selecciona salir o si ocurre un error de comunicación.
 */
void interface() {
    wait_for_ready_signal(); // Esperar hasta que el searcher indique que está listo --> Index cargado
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

    printf("\n\n\n   >⩊< Bienvenido al buscador de canciones por sentimientos ▶︎ •\n");

    // Opciones de búsqueda ingresadas
    char emotion[MAX_FIELD] = "";
    char artist[MAX_FIELD] = "";
    int arousal = -1;

    while (!exit_requested) {
        mostrarMenuPrincipal();

        char choice_str[10];
        if (!fgets(choice_str, sizeof(choice_str), stdin)) break;

        int op = atoi(choice_str);
        if (op == 9) break;

        if (op == 2) {
            printf("\n💬 Ingrese una emoción para buscar ❤️: ");
            if (!fgets(emotion, sizeof(emotion), stdin)) continue;
            emotion[strcspn(emotion, "\n")] = '\0';
            sanitize_input(emotion);
        }
        else if (op == 3) {
            printf("\n🎤 Ingrese el nombre del artista 🎤: ");
            if (!fgets(artist, sizeof(artist), stdin)) continue;
            artist[strcspn(artist, "\n")] = '\0';
            sanitize_input(artist);
        }
        else if (op == 1) {
            printf("\n🎚️ Ingrese la intensidad de la emoción (0 a 100) 🎚️: ");
            if (!fgets(choice_str, sizeof(choice_str), stdin)) continue;
            arousal = atoi(choice_str);
            if (arousal < 0 || arousal > 100) {
                printf("❌ Error: Arousal debe ser un número entre 0 y 100.\n");
                arousal = -1;
                continue;
            }
        }
        else if (op == 4) {
            if (strlen(emotion) == 0 || strlen(artist) == 0 || arousal == -1) {
                printf("❌ Error: Debes ingresar emoción, artista e intensidad antes de realizar la búsqueda.\n");
                continue;
            }

            // Generar la clave completa para la búsqueda
            char search_key[MAX_KEY];
            snprintf(search_key, sizeof(search_key), "%d_%s_%s", arousal, emotion, artist);
            printf("\n🔍 Buscando: '%s'...\n", search_key);

            // Enviar los datos al searcher
            if (write(fd_req, search_key, strlen(search_key)) == -1) {
                if (errno != EPIPE) perror("Error al enviar búsqueda");
                break;  // El searcher probablemente ha muerto
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

            // Recibir y mostrar los resultados
            int song_count = 0;
            while (!exit_requested) {
                Song s;
                ssize_t bytes_read = read(fd_res, &s, sizeof(Song));
                if (bytes_read == 0) {
                    printf("[interface] El searcher cerró la conexión.\n");
                    exit_requested = 1;
                    break;
                }
                if (bytes_read < sizeof(Song)) {
                    perror("Error leyendo respuesta");
                    break;
                }
                if (strlen(s.track) == 0) break;  // Terminador

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

/**
 * Prepara el entorno de ejecución del programa.
 *
 * Elimina cualquier archivo FIFO o bandera existente de ejecuciones anteriores y crea
 * los FIFOs necesarios para la comunicación entre procesos (`PIPE_REQ` y `PIPE_RES`).
 * 
 * Si ocurre un error durante la creación de los FIFOs, se aborta la ejecución.
 */
void setup_environment() {
    printf("[main] Limpiando entorno anterior y creando FIFOs...\n");
    
    // Eliminar FIFOs y flag existentes para empezar limpio
    unlink(PIPE_REQ);
    unlink(PIPE_RES);
    unlink(READY_FLAG);
    
    // Crear FIFO para solicitudes
    if (mkfifo(PIPE_REQ, 0666) == -1) {
        if (errno != EEXIST) {
            perror("[main] Error creando PIPE_REQ");
            exit(1);
        }
    }
    printf("[main] FIFO %s creado.\n", PIPE_REQ);

    // Crear FIFO para respuestas
    if (mkfifo(PIPE_RES, 0666) == -1) {
        if (errno != EEXIST) {
            perror("[main] Error creando PIPE_RES");
            unlink(PIPE_REQ);
            exit(1);
        }
    }
    printf("[main] FIFO %s creado.\n", PIPE_RES);
}

/**
 * Función principal del programa.
 *
 * Según el modo especificado (`searcher` o `interface`), ejecuta el proceso correspondiente:
 *  - searcher: crea el entorno, indexa las canciones y espera consultas.
 *  - interface: lanza el menú para que el usuario realice búsquedas.
 *
 * Uso:
 *   ./programa searcher <archivo_csv>
 *   ./programa interface
 *
 * @param argc Número de argumentos
 * @param argv Vector de argumentos (modo y archivo CSV)
 * @return 0 en caso de éxito, 1 en caso de error
 */
int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    if (argc < 2) {
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