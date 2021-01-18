#pragma once
#include <semaphore.h>
#include <unistd.h>
#include <stdbool.h>
#define MS 1000

#define PLAYER_SIGHT 2

enum player_move{
    PM_UP,
    PM_DOWN,
    PM_LEFT,
    PM_RIGHT
};

typedef struct point_t{
    int x;
    int y;
}point;

struct player_t{
    point position;
    point spawn;
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
    char map[PLAYER_SIGHT][PLAYER_SIGHT];
};
