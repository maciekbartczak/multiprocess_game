#pragma once
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include "common.h"

#define MAP_ROWS 25
#define MAP_COLUMNS 51
#define MAX_TREASURES 20


//ncurses color pairs
#define PLAYER_PAIR 1
#define WALL_PAIR 2
#define BUSH_PAIR 3
#define BEAST_PAIR 4
#define TREASURE_PAIR 5
#define CAMPSITE_PAIR 6
#define EMPTY_PAIR 7

enum map_tiles{
    MAP_WALL = '|',
    MAP_BUSH = '#',
    MAP_EMPTY = ' ',
    MAP_RESERVED
};

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
    struct player_t *player[2];
    struct player_t *player_remote;
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
