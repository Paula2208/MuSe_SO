#ifndef INDEXADOR_H
#define INDEXADOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FIELD 128
#define MAX_ARTIST_BUCKETS 211
#define LINE_BUFFER 4096
#define NUM_FIELDS 12
#define MAX_SEEDS 10
#define INDEX_FOLDER "./output/emotions/"

// Nodo de lista enlazada para posiciones en el CSV
typedef struct PosNode {
    long pos;
    struct PosNode *next;
} PosNode;

// Nodo de la tabla hash de artistas
typedef struct ArtistNode {
    char artist[MAX_FIELD];
    PosNode *positions;
    struct ArtistNode *next;
} ArtistNode;

// Array de 101 posiciones (arousal 0-100) con tabla hash de artistas
typedef struct {
    ArtistNode *buckets[MAX_ARTIST_BUCKETS];
} ArousalIndex;

// Tabla principal de emociones, que apunta a un array de arousal por emoción
typedef struct EmotionIndex {
    char emotion[MAX_FIELD];
    ArousalIndex *arousals;
    struct EmotionIndex *next;
} EmotionIndex;

extern EmotionIndex *emotion_index_head;

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

void sanitize_input(char *str);
unsigned int hash_artist(const char *key);
EmotionIndex *get_or_create_emotion(const char *emotion);
void add_position(const char *emotion, int arousal, const char *artist, long pos);
void save_index_to_disk();
void buildIndex(const char *filename);

#endif // INDEXADOR_H
