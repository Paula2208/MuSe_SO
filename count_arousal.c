#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_AROUSAL 101  // De 0 a 100
#define LINE_BUFFER 2048
#define NUM_FIELDS 12

void contar_arousals_por_entero(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("No se pudo abrir el archivo");
        return;
    }

    int arousal_count[MAX_AROUSAL] = {0};

    char line[LINE_BUFFER];
    // Saltar encabezado
    if (!fgets(line, sizeof(line), file)) {
        fclose(file);
        return;
    }

    while (fgets(line, sizeof(line), file)) {
        char *tokens[NUM_FIELDS];
        char *p_line = line;

        for (int i = 0; i < NUM_FIELDS; i++) {
            char *start = p_line;

            while (*p_line && *p_line != ',') {
                if (*p_line == '[') {
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

        if (tokens[6]) {
            double arousal = atof(tokens[6]);
            int arousal_int = (int)arousal;

            if (arousal_int >= 0 && arousal_int < MAX_AROUSAL) {
                arousal_count[arousal_int]++;
            }
        }
    }

    fclose(file);

    printf("\n🎚️  Canciones por valor entero de arousal:\n");
    for (int i = 0; i < MAX_AROUSAL; i++) {
        if (arousal_count[i] > 0) {
            printf("Arousal %2d: %d canciones\n", i, arousal_count[i]);
        }
    }
}

int main() {

    contar_arousals_por_entero("./Data/muse4gb.csv");
    return 0;
}