#include "server.h"
#include <stdio.h>
#include <stdbool.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

char map[MAP_ROWS][MAP_COLUMNS];
struct game_data_t game_data;


int main(void){
    srand(time(NULL));

    initscr();
    noecho();
    curs_set(0);
    
    if(!load_map("_map.txt")){
        return 1;
    }
    system("rm /dev/shm/player");
    int player2_fd = shm_open("player",O_CREAT | O_EXCL | O_RDWR,0666);
    if(player2_fd == -1){
        perror("shm_open");
        return 1;
    }
    ftruncate(player2_fd,sizeof(struct player_t));

    pthread_mutex_init(&game_data.mutex,NULL);

    struct player_t player_local = {.in_game = false};
    game_data.player[0] = &player_local;
    game_data.player[1] = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,player2_fd,0);
    game_data.player[1]->in_game = false;
    initialize_player(game_data.player[0]);
    game_data.round = 0;
    game_data.campsite = get_empty_tile();
    map[game_data.campsite.y][game_data.campsite.x] = MAP_RESERVED;
    for(int i=0;i<MAX_TREASURES;i++){
        game_data.treasures[i].type = TR_NONE;
    }
    game_data.player[0]->pid = getpid();
    game_data.player[0]->server_pid = game_data.player[0]->pid;
    game_data.player[1]->server_pid = game_data.player[0]->server_pid;

    sem_init(&game_data.player[1]->join_request,1,0);
    sem_init(&game_data.player[1]->join_reply,1,0);
    sem_init(&game_data.player[1]->leave_request,1,0);
    sem_init(&game_data.player[1]->leave_reply,1,0);
    sem_init(&game_data.player[1]->move_request,1,0);
    sem_init(&game_data.player[1]->move_reply,1,0);
    sem_init(&game_data.player[0]->move_request,0,0);
    sem_init(&game_data.player[0]->move_reply,0,0);
    sem_init(&game_data.round_up,0,0);

    pthread_t th_map,th_keyboard,th_join,th_leave,th_round;
    pthread_create(&th_map,NULL,print_map,NULL);
    pthread_create(&th_keyboard,NULL,keyboard_event,NULL);
    pthread_create(&th_join,NULL,player_join,NULL);
    pthread_create(&th_leave,NULL,player_leave,NULL);
    pthread_create(&th_round,NULL,advance_round,NULL);

    pthread_join(th_keyboard,NULL);

    pthread_mutex_destroy(&game_data.mutex);
    munmap(&game_data.player[1],sizeof(struct player_t));
    shm_unlink("player");
    close(player2_fd);

    sem_destroy(&game_data.player[1]->join_request);
    sem_destroy(&game_data.player[1]->leave_request);
    sem_destroy(&game_data.player[1]->join_reply);
    sem_destroy(&game_data.player[1]->leave_request);
    sem_destroy(&game_data.player[1]->leave_reply);
    sem_destroy(&game_data.player[1]->move_request);
    sem_destroy(&game_data.player[1]->move_reply);
    sem_destroy(&game_data.player[0]->move_request);
    sem_destroy(&game_data.player[0]->move_reply);
    sem_destroy(&game_data.round_up);
    endwin();
    return 0;
}


bool load_map(const char *filename){
    if(filename == NULL){
        perror("Filename not provided");
        return false;
    }
    FILE *f = fopen(filename,"r");
    if(f == NULL){
        perror("Couldn't open file");
        return false;
    }
    int i = 0;
    while(!feof(f)){
        fscanf(f,"%s",map[i++]);
    }
    fclose(f);
    for(i = 0; i < MAP_ROWS; i++){
        for(int j = 0; j < MAP_COLUMNS; j++){
            if(map[i][j] == '-'){
                map[i][j] = MAP_EMPTY;
            }
        }
    }
    return true;
}

void *print_map(__attribute__((unused)) void *arg) {
    start_color();
    init_pair(PLAYER_PAIR,COLOR_WHITE,COLOR_MAGENTA);
    init_pair(WALL_PAIR,COLOR_CYAN,COLOR_CYAN);
    init_pair(BUSH_PAIR,COLOR_BLACK,COLOR_BLUE);
    init_pair(BEAST_PAIR,COLOR_RED,COLOR_BLUE);
    init_pair(TREASURE_PAIR,COLOR_BLACK,COLOR_YELLOW);
    init_pair(CAMPSITE_PAIR,COLOR_YELLOW,COLOR_GREEN);
    init_pair(EMPTY_PAIR,COLOR_BLUE,COLOR_BLUE);

    while(1){
        sem_wait(&game_data.round_up);
        for(int i = 0; i < MAP_ROWS; i++){
            for(int j = 0; j < MAP_COLUMNS; j++){
                switch(map[i][j]){
                    case MAP_WALL:
                        attron(COLOR_PAIR(WALL_PAIR));
                        mvprintw(i, j,"%c",map[i][j]);
                        attroff(COLOR_PAIR(WALL_PAIR));
                        break;
                    case MAP_BUSH:
                        attron(COLOR_PAIR(BUSH_PAIR));
                        mvprintw(i, j,"%c",map[i][j]);
                        attroff(COLOR_PAIR(BUSH_PAIR));
                        break;
                    case MAP_EMPTY:
                        attron(COLOR_PAIR(EMPTY_PAIR));
                        mvprintw(i, j,"%c",map[i][j]);
                        attroff(COLOR_PAIR(EMPTY_PAIR));
                        break;
                    default:
                        break;
                } 
            }
        }
        
        for(int i=0;i<MAX_TREASURES;i++){
            if(game_data.treasures[i].type != TR_NONE){
                attron(COLOR_PAIR(TREASURE_PAIR));
                mvprintw(game_data.treasures[i].position.y,game_data.treasures[i].position.x,
                "%c",game_data.treasures[i].type);
                attroff(COLOR_PAIR(TREASURE_PAIR));
            }
           
        }

        for(int i=0;i<2;i++){
            if(game_data.player[i]->in_game){
                attron(COLOR_PAIR(PLAYER_PAIR));
                mvprintw(game_data.player[i]->position.y,game_data.player[i]->position.x,"%d",i+1);
                attroff(COLOR_PAIR(PLAYER_PAIR));
            }   
            
        }
        
        attron(COLOR_PAIR(CAMPSITE_PAIR));
        mvprintw(game_data.campsite.y,game_data.campsite.x,"A");
        attroff(COLOR_PAIR(CAMPSITE_PAIR));

        mvprintw(0,MAP_COLUMNS + 2,"Server PID: %d",game_data.player[0]->server_pid);
        mvprintw(1,MAP_COLUMNS + 4,"Campsite X\\Y %d\\%d",game_data.campsite.x,game_data.campsite.y);
        mvprintw(2,MAP_COLUMNS + 4,"Round number: %d",game_data.round);
        //TODO: print player stats when structs are ready
        mvprintw(5,MAP_COLUMNS + 2,"Parameter:   Player1  Player2");
        mvprintw(6,MAP_COLUMNS + 4,"PID");
        mvprintw(7,MAP_COLUMNS + 4,"Type");
        mvprintw(8,MAP_COLUMNS + 4,"X\\Y");
        mvprintw(9,MAP_COLUMNS + 4,"Deaths");
        mvprintw(11,MAP_COLUMNS + 2,"Coins");
        mvprintw(12,MAP_COLUMNS + 4,"Carried");
        mvprintw(13,MAP_COLUMNS + 4,"Brought");
        for(int i = 0;i < 2; i++){
            if(game_data.player[i]->in_game){
                mvprintw(6,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i]->pid);
                mvprintw(7,MAP_COLUMNS + 4 + 11 +(i * 9),"HUMAN");
                mvprintw(8,MAP_COLUMNS + 4 + 11 +(i * 9),"%d\\%d",
                         game_data.player[i]->position.x, game_data.player[i]->position.y);
                mvprintw(9,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i]->deaths);
                mvprintw(12,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i]->coins_carried);
                mvprintw(13,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i]->coins_brought);
            }else{
                mvprintw(6,MAP_COLUMNS + 4 + 11 +(i * 9),"-      ");
                mvprintw(7,MAP_COLUMNS + 4 + 11 +(i * 9),"-      ");
                mvprintw(8,MAP_COLUMNS + 4 + 11 +(i * 9),"-\\-   ");
                mvprintw(9,MAP_COLUMNS + 4 + 11 +(i * 9),"-      ");
                mvprintw(12,MAP_COLUMNS + 4 + 11 +(i * 9),"-     ");
                mvprintw(13,MAP_COLUMNS + 4 + 11 +(i * 9),"-     ");
            }
           
        }

        mvprintw(15,MAP_COLUMNS + 2,"Legend: ");
        attron(COLOR_PAIR(PLAYER_PAIR));
        mvprintw(16,MAP_COLUMNS + 4,"12");
        attron(COLOR_PAIR(WALL_PAIR));
        mvprintw(17,MAP_COLUMNS + 4," ");
        attron(COLOR_PAIR(BUSH_PAIR));
        mvprintw(18,MAP_COLUMNS + 4,"#");
        attron(COLOR_PAIR(BEAST_PAIR));
        mvprintw(19,MAP_COLUMNS + 4,"*");
        attron(COLOR_PAIR(TREASURE_PAIR));
        mvprintw(20,MAP_COLUMNS + 4,"c");
        mvprintw(21,MAP_COLUMNS + 4,"t");
        mvprintw(22,MAP_COLUMNS + 4,"T");
        mvprintw(23,MAP_COLUMNS + 4,"D");
        attron(COLOR_PAIR(CAMPSITE_PAIR));
        mvprintw(24,MAP_COLUMNS + 4,"A");
        attroff(COLOR_PAIR(CAMPSITE_PAIR));
        mvprintw(16,MAP_COLUMNS + 10,"- players");
        mvprintw(17,MAP_COLUMNS + 10,"- wall");
        mvprintw(18,MAP_COLUMNS + 10,"- bush");
        mvprintw(19,MAP_COLUMNS + 10,"- beast");
        mvprintw(20,MAP_COLUMNS + 10,"- one coin");
        mvprintw(21,MAP_COLUMNS + 10,"- tresure (10 coins)");
        mvprintw(22,MAP_COLUMNS + 10,"- large treasure (50 coins)");
        mvprintw(23,MAP_COLUMNS + 10,"- dropped treasure");
        mvprintw(24,MAP_COLUMNS + 10,"- campsite");
        refresh();
    }
    return NULL;
}


point get_empty_tile(){
    bool valid = false;
    point p;
    while(1){
        p.x = rand() % MAP_COLUMNS;
        p.y = rand() % MAP_ROWS;
        if(map[p.y][p.x] == MAP_EMPTY){
            valid = true;
        }
        for(int i=0;i<2;i++){
                if(game_data.player[i] != NULL && game_data.player[i]->in_game){
                    if(p.x == game_data.player[i]->position.x && 
                       p.y == game_data.player[i]->position.y){
                           valid = false;
                    }
            }
        }
        if(valid){
            break;
        }
    }
    return p;
}

void *keyboard_event(__attribute__((unused)) void *arg){
    int k;
    while(1){
        bool move = false;
        k = getch();
        switch(k){
            case 'c':
            add_treasure(k,1);
                break;
            case 't':
                add_treasure(k,10);
                break;
            case 'T':
                add_treasure(k,50);
                break;
            case 'q':
                return NULL;
            case 'w':
                game_data.player[0]->move = PM_UP;
                move = true;
                break;
            case 's':
                game_data.player[0]->move = PM_DOWN;
                move = true;
                break;
            case 'a':
                game_data.player[0]->move = PM_LEFT;
                move = true;
                break;
            case 'd':
                game_data.player[0]->move = PM_RIGHT;
                move = true;
                break;
            default:
                break;
        }
        if(move){
            sem_post(&game_data.player[0]->move_request);
            sem_wait(&game_data.player[0]->move_reply);
        }
    }
}


void add_treasure(int k,unsigned int amount){
    int i = 0;
    bool found = false;
    for(;i<MAX_TREASURES;i++){
        if(game_data.treasures[i].type == TR_NONE){
            found = true;
            break;
        }
    }
    if(found){
        game_data.treasures[i].coins = amount;
        game_data.treasures[i].position = get_empty_tile();
        game_data.treasures[i].type = k;
        map[game_data.treasures[i].position.y][game_data.treasures[i].position.x] = MAP_RESERVED;
    }
    
}

void initialize_player(struct player_t *p){
    p->position = get_empty_tile();
    p->spawn = p->position;
    p->in_game = true;
    p->deaths = 0;
    p->coins_carried = 0;
    p->coins_brought = 0;
    p->slowed = false;
}

void *player_join(void *arg){
    while(1){
        sem_wait(&game_data.player[1]->join_request);
        pthread_mutex_lock(&game_data.mutex);
        if(!game_data.player[1]->in_game){
            initialize_player(game_data.player[1]);
        }
        sem_post(&game_data.player[1]->join_reply);
        pthread_mutex_unlock(&game_data.mutex);
    }
}

void *player_leave(void *arg){
    while(1){
        sem_wait(&game_data.player[1]->leave_request);
        pthread_mutex_lock(&game_data.mutex);
        if(game_data.player[1]->in_game){
            game_data.player[1]->in_game = false;
            map[game_data.player[1]->position.y][game_data.player[1]->position.x] = MAP_EMPTY;
        }
        sem_post(&game_data.player[1]->leave_reply);
        pthread_mutex_unlock(&game_data.mutex);
    }
}

void *advance_round(void *arg){
    while(1){
        for(int i=0;i<2;i++){
            if(game_data.player[i]->in_game){
                if(game_data.player[i]->slowed){ //skip a turn
                    game_data.player[i]->slowed = false;
                    continue;
                }
                if(sem_trywait(&game_data.player[i]->move_request) == 0){
                    point pos = game_data.player[i]->position;
                    switch(game_data.player[i]->move){
                        case PM_UP:
                            pos.y--;
                            break;
                        case PM_DOWN:
                            pos.y++;
                            break;
                        case PM_LEFT:
                            pos.x--;
                            break;
                        case PM_RIGHT:
                            pos.x++;
                            break;     
                    }
                    if(map[pos.y][pos.x] != MAP_WALL){
                        game_data.player[i]->position = pos;
                    }
                    if(map[pos.y][pos.x] == MAP_BUSH){
                        game_data.player[i]->slowed = true;
                    }
                    if(game_data.campsite.x == pos.x && game_data.campsite.y == pos.y){
                        game_data.player[i]->coins_brought += game_data.player[i]->coins_carried;
                        game_data.player[i]->coins_carried = 0;
                    }
                    for(int j=0;j<MAX_TREASURES;j++){
                        if(game_data.treasures[j].type != TR_NONE &&
                        game_data.treasures[j].position.x == pos.x &&
                        game_data.treasures[j].position.y == pos.y){
                            map[game_data.treasures[j].position.y][game_data.treasures[j].position.x] = MAP_EMPTY;
                            game_data.player[i]->coins_carried += game_data.treasures[j].coins;
                            game_data.treasures[j].type = TR_NONE;
                            break;
                        }
                    }
                    sem_post(&game_data.player[i]->move_reply);
                }
            }
            
        }
        usleep(250 * MS);
        game_data.round++;
        sem_post(&game_data.round_up);
    }
}