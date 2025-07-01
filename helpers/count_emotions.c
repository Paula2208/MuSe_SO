#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FIELD 256
#define MAX_EMOTIONS 1000  // Asumimos que no hay más de 1000 emociones distintas
#define LINE_BUFFER 2048
#define EMOTION_FIELD_INDEX 3  // Columna que contiene las emociones

// Sanitiza una emoción (convierte a minúsculas y elimina caracteres no alfabéticos)
void sanitize_input(char *str) {
    char *src = str, *dst = str;
    while (*src) {
        if (isalpha((unsigned char)*src)) {
            *dst = tolower((unsigned char)*src);
            dst++;
        }
        src++;
    }
    *dst = '\0';
}

// Verifica si la emoción ya está en la lista
int contains(char emotions[][MAX_FIELD], int count, const char *target) {
    for (int i = 0; i < count; i++) {
        if (strcmp(emotions[i], target) == 0)
            return 1;
    }
    return 0;
}

int main() {
    FILE *file = fopen("Data/muse4gb.csv", "r");
    if (!file) {
        perror("No se pudo abrir el archivo CSV");
        return 1;
    }

    char line[LINE_BUFFER];
    char emotions[MAX_EMOTIONS][MAX_FIELD];
    int emotion_count = 0;

    // Saltar encabezado
    if (!fgets(line, sizeof(line), file)) {
        perror("Error al leer encabezado");
        fclose(file);
        return 1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *tokens[12] = {0};
        char *p_line = line;

        for (int i = 0; i < 12; i++) {
            char *start = p_line;
            while (*p_line && *p_line != ',') {
                if (*p_line == '[') while (*p_line && *p_line != ']') p_line++;
                if (*p_line) p_line++;
            }

            if (*p_line) {
                *p_line = '\0';
                p_line++;
            }

            tokens[i] = start;
        }

        if (!tokens[EMOTION_FIELD_INDEX]) continue;

        char seeds_raw[MAX_FIELD];
        snprintf(seeds_raw, sizeof(seeds_raw), "%s", tokens[EMOTION_FIELD_INDEX]);

        // Extraer emociones entre comillas simples
        char *p = seeds_raw;
        while ((p = strchr(p, '\'')) != NULL) {
            p++;
            char *end = strchr(p, '\'');
            if (!end) break;

            *end = '\0';
            char emotion_clean[MAX_FIELD];
            snprintf(emotion_clean, sizeof(emotion_clean), "%s", p);
            sanitize_input(emotion_clean);

            if (strlen(emotion_clean) > 0 && !contains(emotions, emotion_count, emotion_clean)) {
                snprintf(emotions[emotion_count++], MAX_FIELD, "%s", emotion_clean);
                if (emotion_count >= MAX_EMOTIONS) break;
            }

            p = end + 1;
        }
    }

    fclose(file);

    printf("🎭 Total de emociones distintas encontradas: %d\n", emotion_count);
    for (int i = 0; i < emotion_count; i++) {
        printf(" - %s\n", emotions[i]);
    }

    return 0;
}
