#pragma once
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>

#define MS 1000
#define MAP_ROWS 25
#define MAP_COLUMNS 51
#define PLAYER_SIGHT 2

//ncurses color pairs
#define PLAYER_PAIR 1
#define WALL_PAIR 2
#define BUSH_PAIR 3
#define BEAST_PAIR 4
#define TREASURE_PAIR 5
#define CAMPSITE_PAIR 6
#define EMPTY_PAIR 7
#define NONE_PAIR 8

enum player_move{
    PM_UP,
    PM_DOWN,
    PM_LEFT,
    PM_RIGHT
};

enum map_tiles{
    MAP_WALL = '|',
    MAP_BUSH = '#',
    MAP_EMPTY = ' ',
    MAP_CAMPSITE = 'A',
    MAP_TR_COIN = 'c',
    MAP_TR_TREASURE = 't',
    MAP_TR_TREASURE_LARGE = 'T',
    MAP_TR_PLAYER = 'D',
    MAP_NONE
};


typedef struct point_t{
    int x;
    int y;
}point;

struct player_t{
    point position;
    point spawn;
    unsigned int round;
    unsigned int deaths;
    unsigned int coins_carried;
    unsigned int coins_brought;
    pid_t pid;
    pid_t server_pid;
    bool in_game;
    sem_t join_request;
    sem_t join_reply;
    sem_t leave_request;
    sem_t leave_reply;
    enum player_move move;
    sem_t move_request;
    sem_t move_reply;
    bool slowed;
    char map[MAP_ROWS][MAP_COLUMNS];
};
