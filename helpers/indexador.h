#ifndef INDEXADOR_H
#define INDEXADOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_FIELD 128
#define MAX_SEEDS 10
#define MAX_ARTIST_BUCKETS 211
#define INDEX_FOLDER "./output/emotions/"
#define LINE_BUFFER 4096
#define NUM_FIELDS 12
#define CHUNK_SIZE 500000
#define NUM_THREADS 6

// Estructura de una posición en el archivo
typedef struct PosNode {
    long pos;
    struct PosNode *next;
} PosNode;

// Nodo de artista con su lista de posiciones
typedef struct ArtistNode {
    char artist[MAX_FIELD];
    PosNode *positions;
    struct ArtistNode *next;
} ArtistNode;

// Índice por arousal: tabla hash de artistas
typedef struct {
    ArtistNode *buckets[MAX_ARTIST_BUCKETS];
} ArousalIndex;

// Índice por emoción: contiene los niveles de arousal
typedef struct EmotionIndex {
    char emotion[MAX_FIELD];
    ArousalIndex *arousals;
    struct EmotionIndex *next;
} EmotionIndex;

// Estructura para pasar a cada hilo
typedef struct {
    int id;
    char **lines;
    long *positions;
    long num_lines;
} ThreadArgs;

// Estructura que representa una canción (puedes usarla si la necesitas en otras partes del programa)
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

// Variable global que contiene la cabeza del índice
extern EmotionIndex *emotion_index_head;

// Mutex global para proteger acceso concurrente al índice
extern pthread_mutex_t index_mutex;

// Función principal para construir el índice
void buildIndex(const char *filename);

// Guarda el índice completo en disco
void save_index_to_disk(void);

// Sanitiza un string (deja solo letras en minúscula)
void sanitize_input(char *str);

// Hash para el nombre de un artista
unsigned int hash_artist(const char *key);

// Devuelve el índice de una emoción o lo crea si no existe
EmotionIndex *get_or_create_emotion(const char *emotion);

// Agrega una posición al índice para una emoción, arousal y artista dados
void add_position(const char *emotion, int arousal, const char *artist, long pos);

#endif
