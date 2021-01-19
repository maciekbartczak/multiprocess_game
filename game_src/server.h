#pragma once
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include "common.h"

#define MAX_TREASURES 20



enum treasure_type{
    TR_NONE,
    TR_COIN = 'c',
    TR_TREASURE = 't',
    TR_TREASURE_LARGE = 'T',
    TR_PLAYER = 'D'
};


struct treasure_t{
    point position;
    enum treasure_type type;
    unsigned int coins;
};


struct game_data_t{
    pthread_mutex_t mutex;
    point campsite;
    unsigned int round;
    struct treasure_t treasures[MAX_TREASURES];
    struct player_t *player;
    struct player_t *player_remote[2];
    sem_t round_up;
};

bool load_map(const char *filename);
point get_empty_tile();
void add_treasure(int k,unsigned int amount);
void *print_map( void *arg);
void *keyboard_event(void *arg);
void *player_join(void *arg);
void *player_leave(void *arg);
void *advance_round(void *arg);
void initialize_player(struct player_t *p);
