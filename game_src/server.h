#pragma once
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>
#include "common.h"

#define MAX_TREASURES 50
#define MAX_BEAST 10

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

struct beast_t{
    point position;
    sem_t beast_move_request;
    sem_t beast_move_reply;
    bool in_game;
};

struct game_data_t{
    pthread_mutex_t mutex;
    point campsite;
    unsigned int round;
    struct treasure_t treasures[MAX_TREASURES];
    struct player_t *player;
    struct player_t *player_remote[4];
    struct lobby_t *lobby;
    struct beast_t beast[MAX_BEAST];
    sem_t round_up;
    pid_t pid;
};

bool load_map(const char *filename);
point get_empty_tile();
void add_beast();
void add_treasure(int k,unsigned int amount);
void *print_map( void *arg);
void *keyboard_event(void *arg);
void *player_join(void *arg);
void *player_leave(void *arg);
void *advance_round(void *arg);
void *beast_move(void *arg);
bool player_in_sight(int x0, int y0, int x1, int y1);
bool line_low(int x0, int y0, int x1, int y1);
bool line_high(int x0, int y0, int x1, int y1);
void initialize_player(struct player_t *p);
void add_treasure_player(point pos, unsigned int amount);