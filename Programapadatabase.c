#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LENGTH 2048
#define MAX_FIELD_LENGTH 512
#define TOTAL_SONGS 90001  // Cantidad total de canciones

typedef struct {
    char lastfm_url[MAX_FIELD_LENGTH];
    char track[MAX_FIELD_LENGTH];
    char artist[MAX_FIELD_LENGTH];
    char seeds[MAX_FIELD_LENGTH];
    char genre[MAX_FIELD_LENGTH];
} Song;

// Quitar comillas del texto
void remove_quotes(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (*src != '"') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
}

// Cargar canciones desde el CSV
int load_songs(const char *filename, Song *songs) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Error: No se pudo abrir el archivo '%s'.\n", filename);
        return 0;
    }

    char line[MAX_LINE_LENGTH];
    int count = 0;
    int i;

    // Saltar encabezado
    fgets(line, sizeof(line), file);

    while (fgets(line, sizeof(line), file)) {
        char *token;

        token = strtok(line, ",");
        if (token) strncpy(songs[count].lastfm_url, token, MAX_FIELD_LENGTH);

        token = strtok(NULL, ",");
        if (token) strncpy(songs[count].track, token, MAX_FIELD_LENGTH);

        token = strtok(NULL, ",");
        if (token) strncpy(songs[count].artist, token, MAX_FIELD_LENGTH);

        token = strtok(NULL, ",");
        if (token) strncpy(songs[count].seeds, token, MAX_FIELD_LENGTH);

        for (i = 0; i < 6; i++) {
            strtok(NULL, ",");
        }

        token = strtok(NULL, ",\n");
        if (token) strncpy(songs[count].genre, token, MAX_FIELD_LENGTH);

        remove_quotes(songs[count].lastfm_url);
        remove_quotes(songs[count].track);
        remove_quotes(songs[count].artist);
        remove_quotes(songs[count].seeds);
        remove_quotes(songs[count].genre);

        count++;
    }

    fclose(file);
    return count;
}

int main() {
    Song *songs = malloc(sizeof(Song) * TOTAL_SONGS);
    if (!songs) {
        printf("No se pudo asignar memoria.\n");
        return 1;
    }

    int total = load_songs("muse_v3.csv", songs);
    printf("? Se cargaron %d canciones del archivo.\n", total);

    while (1) {
        int number;
        printf("\nIntroduce el número de la canci\363n (0 a %d) o -1 para salir: ", total - 1);
        scanf("%d", &number);

        if (number == -1) {
            printf("?? Cerrando programa.\n");
            break;
        }

        if (number >= 0 && number < total) {
            printf("\n?? Información de la canción:\n");
            printf("- Número: %d\n", number);
            printf("- Track: %s\n", songs[number].track);
            printf("- Artista: %s\n", songs[number].artist);
            printf("- Género: %s\n", songs[number].genre);
            printf("- Emociones: %s\n", songs[number].seeds);
            printf("- URL LastFM: %s\n", songs[number].lastfm_url);
        } else {
            printf("? Error: Número fuera de rango.\n");
        }
    }

    free(songs);
    return 0;
}

