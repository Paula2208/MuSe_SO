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

#include "indexador.h"

#define PIPE_REQ "./output/search_req.pipe"
#define PIPE_RES "./output/search_res.pipe"
#define READY_FLAG "./output/searcher.ready"

volatile sig_atomic_t exit_requested = 0;

void handle_sigint(int sig) {
    printf("\n\n⏹️  Interrupción detectada (Ctrl+C). Cerrando programa con seguridad...\n");
    exit_requested = 1;
}

EmotionIndex *loadEmotionIndex(const char *emotion) {
    char path[256];
    snprintf(path, sizeof(path), "%sindex_%s.bin", INDEX_FOLDER, emotion);
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror("[loadEmotionIndex] No se pudo abrir archivo binario");
        return NULL;
    }

    EmotionIndex *eidx = malloc(sizeof(EmotionIndex));
    if (!eidx) {
        perror("malloc");
        fclose(file);
        return NULL;
    }
    strncpy(eidx->emotion, emotion, MAX_FIELD-1);
    eidx->emotion[MAX_FIELD - 1] = '\0'; 
    eidx->arousals = calloc(101, sizeof(ArousalIndex));
    eidx->next = emotion_index_head;
    emotion_index_head = eidx;

    for (int i = 0; i <= 100; i++) {
        int artist_count;
        if (fread(&artist_count, sizeof(int), 1, file) != 1) {
            fprintf(stderr, "[loadEmotionIndex] Error leyendo count arousal %d\n", i);
            fclose(file);
            return NULL;
        }

        for (int j = 0; j < artist_count; j++) {
            int len;
            if (fread(&len, sizeof(int), 1, file) != 1) break;

            char artist[MAX_FIELD];
            if (fread(artist, sizeof(char), len, file) != len) break;
            artist[len] = '\0';

            unsigned int h = hash_artist(artist);
            ArtistNode *an = malloc(sizeof(ArtistNode));
            strncpy(an->artist, artist, MAX_FIELD - 1);
            an->artist[MAX_FIELD - 1] = '\0';
            an->positions = NULL;
            an->next = eidx->arousals[i].buckets[h];
            eidx->arousals[i].buckets[h] = an;

            int pos_count;
            if (fread(&pos_count, sizeof(int), 1, file) != 1) {
                fprintf(stderr, "Error leyendo cantidad de posiciones\n");
                fclose(file);
                return NULL;
            }

            for (int k = 0; k < pos_count; k++) {
                long p;
                if (fread(&p, sizeof(long), 1, file) != 1) {
                    fprintf(stderr, "Error leyendo cantidad de posiciones\n");
                    fclose(file);
                    return NULL;
                }

                PosNode *pn = malloc(sizeof(PosNode));
                pn->pos = p;
                pn->next = an->positions;
                an->positions = pn;
            }
        }
    }

    fclose(file);
    printf("[loadEmotionIndex] Índice cargado correctamente para '%s'\n", emotion);
    return eidx;
}

void clearEmotionIndex() {
    EmotionIndex *curr = emotion_index_head;
    while (curr) {
        EmotionIndex *to_free = curr;
        curr = curr->next;

        if (to_free->arousals) {
            for (int i = 0; i <= 100; i++) {
                ArousalIndex *ai = &to_free->arousals[i];
                for (int b = 0; b < MAX_ARTIST_BUCKETS; b++) {
                    ArtistNode *artist = ai->buckets[b];
                    while (artist) {
                        ArtistNode *next_artist = artist->next;

                        PosNode *pos = artist->positions;
                        while (pos) {
                            PosNode *next_pos = pos->next;
                            free(pos);
                            pos = next_pos;
                        }

                        free(artist);
                        artist = next_artist;
                    }
                }
            }
            free(to_free->arousals);
        }

        free(to_free);
    }
    emotion_index_head = NULL;
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

void searcher(const char *csv_path) {
    signal(SIGINT, handle_sigint);
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

    char prev_emotion[MAX_FIELD] = "";
    FILE *index_file = NULL;

    while (!exit_requested) {
        int arousal;
        char emotion[MAX_FIELD];
        char artist[MAX_FIELD];

        ssize_t r1 = read(fd_req, &arousal, sizeof(int));
        ssize_t r2 = read(fd_req, emotion, sizeof(emotion));
        ssize_t r3 = read(fd_req, artist, sizeof(artist));

        if (r1 <= 0 || r2 <= 0 || r3 <= 0) {
            printf("[searcher] Conexión cerrada o error leyendo.\n");
            break;
        }

        sanitize_input(emotion);
        sanitize_input(artist);

        printf("[searcher] Búsqueda: Arousal=%d, Emotion='%s', Artist='%s'\n", arousal, emotion, artist);

        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        // Cargar el índice si cambió la emoción
        if (strcmp(prev_emotion, emotion) != 0) {
            if (index_file) fclose(index_file);
            char index_path[256];
            snprintf(index_path, sizeof(index_path), "%sindex_%s.bin", INDEX_FOLDER, emotion);
            index_file = fopen(index_path, "rb");
            if (!index_file) {
                perror("[searcher] No se pudo abrir índice binario");
                int cero = 0;
                if (write(fd_res, &cero, sizeof(int)) != sizeof(int)) {
                    perror("Error cargando índice de emociones");
                }

                continue;
            }

            clearEmotionIndex();  // limpia estructura global si ya existía
            loadEmotionIndex(emotion);  // reconstruye emotion_index_head
            strncpy(prev_emotion, emotion, MAX_FIELD - 1);
            prev_emotion[MAX_FIELD - 1] = '\0';

        }

        // Buscar directamente usando arousal y artista
        EmotionIndex *eidx = emotion_index_head;
        while (eidx && strcmp(eidx->emotion, emotion) != 0)
            eidx = eidx->next;

        if (!eidx || arousal < 0 || arousal > 100) {
            int cero = 0;
            if (write(fd_res, &cero, sizeof(int)) != sizeof(int)) {
                perror("Error obteniendo arousal");
            }

            continue;
        }

        ArousalIndex *ai = &eidx->arousals[arousal];
        unsigned int h = hash_artist(artist);
        ArtistNode *an = ai->buckets[h];
        while (an && strcmp(an->artist, artist) != 0)
            an = an->next;

        // Contar y recolectar posiciones
        int *positions = NULL;
        size_t found = 0;
        for (PosNode *pn = an ? an->positions : NULL; pn; pn = pn->next) {
            positions = realloc(positions, (found + 1) * sizeof(int));
            positions[found++] = pn->pos;
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        double time_spent = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        printf("\n[searcher] Búsqueda completada: %zu canciones encontradas en %.3f segundos\n", 
               found, time_spent);
               
        if (write(fd_res, &found, sizeof(int)) != sizeof(int)) {
            perror("Error enviando total de canciones encontradas");
        }


        if (found > 0) {
            char confirm;
            if (read(fd_req, &confirm, 1) <= 0 || confirm != 'y') {
                free(positions);
                continue;
            }

            FILE *songs_file = fopen(csv_path, "r");
            if (!songs_file) {
                perror("[searcher] Error abriendo CSV de canciones");
                free(positions);
                continue;
            }

            for (size_t i = 0; i < found; i++) {
                Song s = readSongAt(songs_file, positions[i]);
                if (write(fd_res, &s, sizeof(Song)) != sizeof(Song)) {
                    perror("[searcher] Error escribiendo canción");
                    break;
                }
            }

            Song terminator = {0};
            if (write(fd_res, &terminator, sizeof(Song)) != sizeof(Song)) {
                perror("Error enviando terminador");
            }

            fclose(songs_file);
        }

        free(positions);
    }

    printf("[searcher] Finalizando proceso correctamente...\n");
    if (index_file) fclose(index_file);
    close(fd_req);
    close(fd_res);
    clearEmotionIndex();
    printf("\n¡Hasta pronto! ᡣ • . • 𐭩 ♡\n");
}

void mostrarMenuPrincipal() {
    printf("\n\n====================\n");
    printf("🌟 Menú Principal:\n");
    printf("1. Ingresar emoción ❤️ \n");
    printf("2. Ingresar la intensidad de la emoción (0 a 100)  🎚️\n");
    printf("3. Ingresar el artista 🎤\n");
    printf("4. Realizar la búsqueda\n");
    printf("9. Salir\n");
    printf("Seleccione una opción: ");
}


void interface() {
    wait_for_ready_signal();
    if (exit_requested) return;

    int fd_req = open(PIPE_REQ, O_WRONLY);
    if (fd_req == -1) {
        perror("[interface] Error abriendo PIPE_REQ");
        exit(1);
    }

    int fd_res = open(PIPE_RES, O_RDONLY);
    if (fd_res == -1) {
        perror("[interface] Error abriendo PIPE_RES");
        close(fd_req);
        exit(1);
    }

    printf("\n\n\n   >⩊< Bienvenido al buscador de canciones por sentimientos ▶︎ •\n");

    char emotion[MAX_FIELD] = "";
    char artist[MAX_FIELD] = "";
    int arousal = -1;

    while (!exit_requested) {
        mostrarMenuPrincipal();

        char choice_str[10];
        if (!fgets(choice_str, sizeof(choice_str), stdin)) break;
        int op = atoi(choice_str);
        if (op == 9) break;

        if (op == 1) {
            printf("\n💬 Ingrese una emoción para buscar ❤️ : ");
            if (!fgets(emotion, sizeof(emotion), stdin)) continue;
            emotion[strcspn(emotion, "\n")] = '\0';
            sanitize_input(emotion);
        } else if (op == 3) {
            printf("\n🎤 Ingrese el nombre del artista 🎤: ");
            if (!fgets(artist, sizeof(artist), stdin)) continue;
            artist[strcspn(artist, "\n")] = '\0';
            sanitize_input(artist);
        } else if (op == 2) {
            printf("\n🎚️ Ingrese la intensidad de la emoción (0 a 100) 🎚️: ");
            if (!fgets(choice_str, sizeof(choice_str), stdin)) continue;
            arousal = atoi(choice_str);
            if (arousal < 0 || arousal > 100) {
                printf("❌ Error: Arousal debe ser un número entre 0 y 100.\n");
                arousal = -1;
                continue;
            }
        } else if (op == 4) {
            if (strlen(emotion) == 0 || strlen(artist) == 0 || arousal == -1) {
                printf("❌ Error: Debes ingresar emoción, artista e intensidad antes de realizar la búsqueda.\n");
                continue;
            }

            if (write(fd_req, &arousal, sizeof(int)) != sizeof(int)) {
                perror("Error enviando arousal");
            }

            if (write(fd_req, emotion, sizeof(emotion)) != sizeof(emotion)) {
                perror("Error enviando emotion");
            }

            if (write(fd_req, artist, sizeof(artist)) != sizeof(artist)) {
                perror("Error enviando artist");
            }


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

            char respuesta[4];
            printf("\n🎵 Se encontraron %d canciones. ¿Desea mostrarlas? (s/n): ", total_encontradas);
            if (!fgets(respuesta, sizeof(respuesta), stdin)) continue;
            sanitize_input(respuesta);
            char confirm = (respuesta[0] == 's' || respuesta[0] == 'S') ? 'y' : 'n';

            if (write(fd_req, &confirm, 1) != 1) {
                perror("Error enviando confirmación");
            }

            if (confirm == 'n') {
                printf("📭 Resultados omitidos. Volviendo al menú...\n");
                continue;
            }

            int song_count = 0;
            while (!exit_requested) {
                Song s;
                ssize_t bytes_read = read(fd_res, &s, sizeof(Song));
                if (bytes_read <= 0 || strlen(s.track) == 0) break;
                printSong(s);
                song_count++;
            }
            printf("\n✅ Total de canciones encontradas: %d\n", song_count);
            printf("\n🔁 Volviendo al menú principal...\n");
        }
    }

    printf("[interface] Finalizando proceso correctamente....\n");
    close(fd_req);
    close(fd_res);
    printf("\n¡Hasta pronto! ᡣ • . • 𐭩 ♡\n");
}

void setup_environment() {
    printf("[main] Limpiando entorno anterior y creando FIFOs...\n");
    unlink(PIPE_REQ);
    unlink(PIPE_RES);
    unlink(READY_FLAG);

    if (mkfifo(PIPE_REQ, 0666) == -1 && errno != EEXIST) {
        perror("[main] Error creando PIPE_REQ");
        exit(1);
    }
    if (mkfifo(PIPE_RES, 0666) == -1 && errno != EEXIST) {
        perror("[main] Error creando PIPE_RES");
        unlink(PIPE_REQ);
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <modo> [archivo_csv]\n", argv[0]);
        fprintf(stderr, "Modos disponibles: searcher | interface | indexer\n");
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
    } else if (strcmp(argv[1], "interface") == 0) {
        printf("[main] Ejecutando Interface\n");
        interface();
    } else if (strcmp(argv[1], "indexer") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Uso: %s indexer <archivo_csv>\n", argv[0]);
            return 1;
        }
        printf("[main] Ejecutando Indexador\n");
        buildIndex(argv[2]);
    } else {
        fprintf(stderr, "Modo desconocido: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
