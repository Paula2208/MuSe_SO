// client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>

#include "./helpers/indexador.h"

#define PORT 3550
#define HOST "34.44.216.84" // "127.0.0.1" // Cambiar a la IP del servidor si es necesario

int clientfd = -1;

// Manejador de Ctrl+C
void handle_sigint(int sig) {
    printf("\n🔴 Señal SIGINT recibida. Cerrando cliente...\n");
    if (clientfd != -1) close(clientfd);
    printf("🔌 Cliente cerrado.\n");
    exit(0);
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
    printf("\n\n====================\n");
    printf("🌟 Menú Principal:\n");
    printf("1. Ingresar emoción ❤️ \n");
    printf("2. Ingresar la intensidad (0-100) 🎚️\n");
    printf("3. Ingresar el artista 🎤\n");
    printf("4. Realizar la búsqueda 🔍\n");
    printf("9. Salir\n");
    printf("Seleccione una opción: ");
}


int main() {
    struct sockaddr_in server;
    signal(SIGINT, handle_sigint);

    // 1. Crear Socket
    clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1) {
        perror("❌ Error creando socket");
        exit(EXIT_FAILURE);
    }

    // 2. Conectar al servidor
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(HOST);
    memset(server.sin_zero, 0, sizeof(server.sin_zero));

    if (connect(clientfd, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("❌ Error conectando al servidor");
        exit(EXIT_FAILURE);
    }

    printf("✅ Conectado al servidor (Searcher) en %s:%d\n", HOST, PORT);
    printf("\n\n\n   >⩊< Bienvenido al buscador de canciones por sentimientos ▶︎ •\n");

    // --- Lógica de la Interface ---
    char emotion[MAX_FIELD] = "";
    char artist[MAX_FIELD] = "";
    int arousal = -1;

    while (1) {
        mostrarMenuPrincipal();
        char choice_str[10];
        if (!fgets(choice_str, sizeof(choice_str), stdin)) break;
        int op = atoi(choice_str);

        if (op == 9) break;

        switch(op) {
            case 1:
                printf("\n💬 Ingrese una emoción para buscar ❤️ : ");
                if (!fgets(emotion, sizeof(emotion), stdin)) continue;
                emotion[strcspn(emotion, "\n")] = '\0';
                sanitize_input(emotion);
                break;
            case 2:
                printf("\n🎚️ Ingrese la intensidad (0 a 100) 🎚️: ");
                if (!fgets(choice_str, sizeof(choice_str), stdin)) continue;
                arousal = atoi(choice_str);
                if (arousal < 0 || arousal > 100) {
                    printf("❌ Error: Arousal debe ser un número entre 0 y 100.\n");
                    arousal = -1;
                }
                break;
            case 3:
                printf("\n🎤 Ingrese el nombre del artista 🎤: ");
                if (!fgets(artist, sizeof(artist), stdin)) continue;
                artist[strcspn(artist, "\n")] = '\0';
                sanitize_input(artist);
                break;
            case 4:
                if (strlen(emotion) == 0 || strlen(artist) == 0 || arousal == -1) {
                    printf("❌ Error: Debes ingresar emoción, artista e intensidad antes de buscar.\n");
                    continue;
                }
                
                // Enviar datos al servidor
                send(clientfd, &arousal, sizeof(int), 0);
                send(clientfd, emotion, sizeof(emotion), 0);
                send(clientfd, artist, sizeof(artist), 0);

                // Recibir cantidad de resultados
                long total_encontradas = 0;
                if (recv(clientfd, &total_encontradas, sizeof(long), 0) <= 0) {
                    printf("❌ Error recibiendo datos del servidor.\n");
                    break;
                }

                if (total_encontradas == 0) {
                    printf("\n❌ No se encontraron canciones con ese criterio.\n");
                    continue;
                }

                printf("\n🎵 Se encontraron %ld canciones. ¿Desea mostrarlas? (s/n): ", total_encontradas);
                char respuesta[4];
                if (!fgets(respuesta, sizeof(respuesta), stdin)) continue;
                char confirm = (tolower(respuesta[0]) == 's') ? 'y' : 'n';

                send(clientfd, &confirm, 1, 0); // Enviar confirmación

                if (confirm == 'y') {
                    int song_count = 0;
                    while (1) {
                        Song s;
                        if (recv(clientfd, &s, sizeof(Song), 0) <= 0) break;
                        if (strlen(s.track) == 0) break; // Es el terminador
                        printSong(s);
                        song_count++;
                    }
                     printf("\n✅ Total de canciones mostradas: %d\n", song_count);
                } else {
                     printf("📭 Resultados omitidos. Volviendo al menú...\n");
                }
                break;
            default:
                printf("❌ Opción no válida.\n");
        }
    }

    printf("👋 Cerrando conexión con el servidor.\n");
    close(clientfd);
    return 0;
}