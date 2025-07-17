// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>

#include "./helpers/indexador.h"

#define PORT 3550
#define BACKLOG 8

// Sockets file descriptors
int serverfd = -1;
int clientfd = -1;

void clearEmotionIndex();

// Manejador de Ctrl+C
void handle_sigint(int sig) {
    printf("\n🔴 Señal SIGINT recibida. Cerrando servidor...\n");

    if (clientfd != -1) close(clientfd);
    if (serverfd != -1) close(serverfd);
    
    clearEmotionIndex();
    printf("🛑 Servidor cerrado de forma segura.\n");
    exit(0);
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


// --- Inicio del código del servidor ---

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <archivo_csv>\n", argv[0]);
        return 1;
    }
    const char *csv_path = argv[1];

    struct sockaddr_in server, client;
    socklen_t lenclient;
    int opt = 1;

    signal(SIGINT, handle_sigint);

    // 1. Creación del Socket
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd == -1) {
        perror("❌ Error creando socket del servidor");
        exit(EXIT_FAILURE);
    }
    setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Configuración del Bind
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(server.sin_zero, 0, sizeof(server.sin_zero));

    if (bind(serverfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("❌ Error al hacer bind");
        exit(EXIT_FAILURE);
    }

    // 3. Poner en escucha
    if (listen(serverfd, BACKLOG) == -1) {
        perror("❌ Error al poner en escucha");
        exit(EXIT_FAILURE);
    }

    printf("🚀 Servidor (Searcher) escuchando en el puerto %d...\n", PORT);

    // 4. Aceptar conexión del cliente (Interface)
    lenclient = sizeof(client);
    clientfd = accept(serverfd, (struct sockaddr *)&client, &lenclient);
    if (clientfd == -1) {
        perror("❌ Error al aceptar conexión");
        exit(EXIT_FAILURE);
    }
    printf("✅ Cliente (Interface) conectado desde %s:%d\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

    // --- Lógica del Searcher ---
    char prev_emotion[MAX_FIELD] = "";

    while (1) {
        int arousal;
        char emotion[MAX_FIELD];
        char artist[MAX_FIELD];

        // Recibir datos de búsqueda del cliente
        if (recv(clientfd, &arousal, sizeof(int), 0) <= 0) break;
        if (recv(clientfd, emotion, sizeof(emotion), 0) <= 0) break;
        if (recv(clientfd, artist, sizeof(artist), 0) <= 0) break;
        
        printf("[searcher] Búsqueda recibida: Arousal=%d, Emotion='%s', Artist='%s'\n", arousal, emotion, artist);

        // Cargar índice si es necesario
        if (strcmp(prev_emotion, emotion) != 0) {
            clearEmotionIndex();
            loadEmotionIndex(emotion);
            /*strncpy(prev_emotion, emotion, MAX_FIELD - 1);
            prev_emotion[MAX_FIELD-1] = '\0';*/
            snprintf(prev_emotion, MAX_FIELD, "%s", emotion);
        }
        
        // Buscar en el índice
        EmotionIndex* eidx = emotion_index_head; // Asume que loadEmotionIndex la popula
        ArousalIndex* ai = (eidx && arousal >= 0 && arousal <= 100) ? &eidx->arousals[arousal] : NULL;
        unsigned int h = hash_artist(artist);
        ArtistNode* an = ai ? ai->buckets[h] : NULL;
        while(an && strcmp(an->artist, artist) != 0) an = an->next;

        long found = 0;
        PosNode* positions_head = an ? an->positions : NULL;
        for (PosNode* pn = positions_head; pn; pn = pn->next) found++;

        // Enviar cantidad de resultados al cliente
        send(clientfd, &found, sizeof(long), 0);
        printf("[searcher] Se encontraron %ld canciones. Enviando recuento al cliente.\n", found);

        if (found > 0) {
            char confirm;
            if (recv(clientfd, &confirm, 1, 0) <= 0 || confirm != 'y') {
                printf("[searcher] El cliente no quiere ver los resultados.\n");
                continue;
            }
            
            printf("[searcher] El cliente confirmó. Enviando canciones...\n");
            FILE *songs_file = fopen(csv_path, "r");
            if (!songs_file) {
                perror("[searcher] Error abriendo CSV de canciones");
                continue;
            }

            for (PosNode* pn = positions_head; pn; pn = pn->next) {
                Song s = readSongAt(songs_file, pn->pos);
                send(clientfd, &s, sizeof(Song), 0);
            }
            
            Song terminator = {0}; // Canción vacía para marcar el final
            send(clientfd, &terminator, sizeof(Song), 0);
            fclose(songs_file);
            printf("[searcher] Envío de canciones completado.\n");
        }
    }

    printf("❌ Cliente desconectado. Cerrando servidor.\n");
    close(clientfd);
    close(serverfd);
    clearEmotionIndex();
    return 0;
}