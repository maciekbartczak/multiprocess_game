#include "server.h"
#include <stdio.h>
#include <ncurses.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>

char map[MAP_ROWS][MAP_COLUMNS];
char map_clean[MAP_ROWS][MAP_COLUMNS];
struct game_data_t game_data;
pthread_t th_beast[MAX_BEAST];
unsigned int beast_id[10];
unsigned int beast_count = 0;

int main(void){
    srand(time(NULL));

    initscr();
    noecho();
    curs_set(0);
    
    if(!load_map("_map.txt")){
        return 1;
    }
    for(int i=0;i<MAP_ROWS;i++){
        for(int j=0;j<MAP_COLUMNS;j++){
            map_clean[i][j] = map[i][j];
        }
    }
    int lobby_fd = shm_open("lobby",O_CREAT | O_EXCL | O_RDWR,0666);
    int player1_fd = shm_open("player1",O_CREAT | O_EXCL | O_RDWR,0666);
    int player2_fd = shm_open("player2",O_CREAT | O_EXCL | O_RDWR,0666);
    int player3_fd = shm_open("player3",O_CREAT | O_EXCL | O_RDWR,0666);
    int player4_fd = shm_open("player4",O_CREAT | O_EXCL | O_RDWR,0666);
    ftruncate(lobby_fd,sizeof(struct lobby_t));
    ftruncate(player1_fd,sizeof(struct player_t));
    ftruncate(player2_fd,sizeof(struct player_t));
    ftruncate(player3_fd,sizeof(struct player_t));
    ftruncate(player4_fd,sizeof(struct player_t));

    pthread_mutex_init(&game_data.mutex,NULL);

    struct player_t player_local[4] = {[0].in_game = false, [1].in_game = false,
                                       [2].in_game = false,[3].in_game = false};
    game_data.player = player_local;
    game_data.player_remote[0] = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,player1_fd,0); 
    game_data.player_remote[1] = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,player2_fd,0);
    game_data.player_remote[2] = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,player3_fd,0);
    game_data.player_remote[3] = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,player4_fd,0);
    game_data.lobby = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,lobby_fd,0);
    for(int i=0;i<4;i++){
        game_data.lobby->slot[i] = 0;
    }
    game_data.round = 0;
    game_data.campsite = get_empty_tile();
    map[game_data.campsite.y][game_data.campsite.x] = MAP_CAMPSITE;
    for(int i=0;i<MAX_TREASURES;i++){
        game_data.treasures[i].type = TR_NONE;
    }
    game_data.pid = getpid();
    game_data.lobby->server_pid = game_data.pid;

    const char treasures[] = "ctT";
    const unsigned int amounts[] = {1,10,50};
    for(int i=0;i<10;i++){
        int t = rand()%3;
        add_treasure(treasures[t],amounts[t]);
    }
    for(int i=0;i<4;i++){
        sem_init(&game_data.player_remote[i]->move_request,1,0);
        sem_init(&game_data.player_remote[i]->move_reply,1,0);
    }
    sem_init(&game_data.lobby->leave_request,1,0);
    sem_init(&game_data.lobby->leave_reply,1,0);
    sem_init(&game_data.lobby->join_request,1,0);
    sem_init(&game_data.lobby->join_reply,1,0);
    sem_init(&game_data.round_up,0,0);
  

    pthread_t th_map,th_keyboard,th_join,th_leave,th_round;
    pthread_create(&th_map,NULL,print_map,NULL);
    pthread_create(&th_keyboard,NULL,keyboard_event,NULL);
    pthread_create(&th_join,NULL,player_join,NULL);
    pthread_create(&th_leave,NULL,player_leave,NULL);
    pthread_create(&th_round,NULL,advance_round,NULL);

    pthread_join(th_keyboard,NULL);

    pthread_mutex_destroy(&game_data.mutex);
    for(int i=0;i<4;i++){
        munmap(&game_data.player_remote[i],sizeof(struct player_t));
    }
    
    shm_unlink("lobby");
    shm_unlink("player1");
    shm_unlink("player2");
    shm_unlink("player3");
    shm_unlink("player4");
    close(lobby_fd);
    close(player1_fd);
    close(player2_fd);
    close(player3_fd);
    close(player4_fd);

    for(int i=0;i<4;i++){
        sem_destroy(&game_data.player_remote[i]->move_request);
        sem_destroy(&game_data.player_remote[i]->move_reply);
    }
    sem_destroy(&game_data.lobby->leave_request);
    sem_destroy(&game_data.lobby->leave_reply);
    sem_destroy(&game_data.lobby->join_request);
    sem_destroy(&game_data.lobby->join_reply);
    sem_destroy(&game_data.round_up);
    for(int i=0;i<MAX_BEAST;i++){
        if(game_data.beast[i].in_game){
            sem_destroy(&game_data.beast[i].beast_move_request);
            sem_destroy(&game_data.beast[i].beast_move_reply);
        }   
  
    }
    
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
                    case MAP_TR_COIN:
                    case MAP_TR_TREASURE:
                    case MAP_TR_TREASURE_LARGE:
                    case MAP_TR_PLAYER:
                        attron(COLOR_PAIR(TREASURE_PAIR));
                        mvprintw(i,j,"%c",map[i][j]);
                        attroff(COLOR_PAIR(TREASURE_PAIR));
                        break;
                    case MAP_CAMPSITE:
                        attron(COLOR_PAIR(CAMPSITE_PAIR));
                        mvprintw(i,j,"%c",map[i][j]);
                        attroff(COLOR_PAIR(CAMPSITE_PAIR));
                        break;
                    default:
                        break;
                } 
            }
        }

        for(int i=0;i<4;i++){
            if(game_data.player[i].in_game){
                attron(COLOR_PAIR(PLAYER_PAIR));
                mvprintw(game_data.player[i].position.y,game_data.player[i].position.x,"%d",i+1);
                attroff(COLOR_PAIR(PLAYER_PAIR));
            }   
        }

        for(int i=0;i<MAX_BEAST;i++){
            if(game_data.beast[i].in_game){
                attron(COLOR_PAIR(BEAST_PAIR));
                mvprintw(game_data.beast[i].position.y,game_data.beast[i].position.x,"*");
                attroff(COLOR_PAIR(BEAST_PAIR));
            }
        }

        mvprintw(0,MAP_COLUMNS + 2,"Server PID: %d",game_data.pid);
        mvprintw(1,MAP_COLUMNS + 4,"Campsite X\\Y %d\\%d",game_data.campsite.x,game_data.campsite.y);
        mvprintw(2,MAP_COLUMNS + 4,"Round number: %d",game_data.round);
        mvprintw(5,MAP_COLUMNS + 2,"Parameter:   Player1  Player2  Player3  Player4");
        mvprintw(6,MAP_COLUMNS + 4,"PID");
        mvprintw(7,MAP_COLUMNS + 4,"Type");
        mvprintw(8,MAP_COLUMNS + 4,"X\\Y");
        mvprintw(9,MAP_COLUMNS + 4,"Deaths");
        mvprintw(11,MAP_COLUMNS + 2,"Coins");
        mvprintw(12,MAP_COLUMNS + 4,"Carried");
        mvprintw(13,MAP_COLUMNS + 4,"Brought");
        for(int i = 0;i < 4;i++){
            if(game_data.player[i].in_game){
                mvprintw(6,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player_remote[i]->pid);
                mvprintw(7,MAP_COLUMNS + 4 + 11 +(i * 9),"HUMAN");
                mvprintw(8,MAP_COLUMNS + 4 + 11 +(i * 9),"%02d\\%02d",
                         game_data.player[i].position.x, game_data.player[i].position.y);
                mvprintw(9,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i].deaths);
                if(game_data.player[i].coins_carried == 0){
                     mvprintw(12,MAP_COLUMNS + 4 + 11 +(i * 9),"%d   ",game_data.player[i].coins_carried);
                }else{
                    mvprintw(12,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i].coins_carried);
                }
                mvprintw(13,MAP_COLUMNS + 4 + 11 +(i * 9),"%d",game_data.player[i].coins_brought);
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
        mvprintw(16,MAP_COLUMNS + 4,"1234");
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
        mvprintw(21,MAP_COLUMNS + 10,"- treasure (10 coins)");
        mvprintw(22,MAP_COLUMNS + 10,"- large treasure (50 coins)");
        mvprintw(23,MAP_COLUMNS + 10,"- dropped treasure");
        mvprintw(24,MAP_COLUMNS + 10,"- campsite");
        refresh();
    }
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
        for(int i=0;i<4;i++){
                if(game_data.player[i].in_game){
                    if(p.x == game_data.player[i].position.x && 
                       p.y == game_data.player[i].position.y){
                           valid = false;
                    }
            }
        }
        for(int i=0;i<MAX_BEAST;i++){
            if(game_data.beast[i].in_game){
                if(p.x == game_data.beast[i].position.x &&
                   p.y == game_data.beast[i].position.y){
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
        k = getchar();
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
            case 'b':
                add_beast();
                break;
            case 'q':
                return NULL;
            default:
                break;
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
        char tr_type;
        switch(k){
            case TR_COIN:
                tr_type = MAP_TR_COIN;
                break;
            case TR_TREASURE:
                tr_type = MAP_TR_TREASURE;
                break;
            case TR_TREASURE_LARGE:
                tr_type = MAP_TR_TREASURE_LARGE;
                break;
            default:
                tr_type = TR_NONE;
        }
        map[game_data.treasures[i].position.y][game_data.treasures[i].position.x] = tr_type;
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
        sem_wait(&game_data.lobby->join_request);
        pthread_mutex_lock(&game_data.mutex);
        int slot_free=0;
        bool found = false;
        for(;slot_free<4;slot_free++){
            if(!game_data.player[slot_free].in_game){
                initialize_player(&game_data.player[slot_free]);
                game_data.player_remote[slot_free]->position = game_data.player[slot_free].position;
                game_data.player_remote[slot_free]->in_game = game_data.player[slot_free].in_game;
                game_data.player_remote[slot_free]->deaths = game_data.player[slot_free].deaths;
                game_data.player_remote[slot_free]->coins_carried = game_data.player[slot_free].coins_carried;
                game_data.player_remote[slot_free]->coins_brought = game_data.player[slot_free].coins_brought;
                game_data.lobby->slot[slot_free] = -1;
                found = true;
            }
            if(found){
                break;
            }
        }
        sem_post(&game_data.lobby->join_reply);
        pthread_mutex_unlock(&game_data.mutex);
    }
}

void *player_leave(void *arg){
    while(1){
        sem_wait(&game_data.lobby->leave_request);
        pthread_mutex_lock(&game_data.mutex);
        int slot=0;
        for(;slot<4;slot++){
            if(game_data.lobby->slot[slot] == -1){
                if(game_data.player[slot].in_game){
                    game_data.player[slot].in_game = false;
                    game_data.lobby->slot[slot] = 0;
                }
                break;
            }
        }
        sem_post(&game_data.lobby->leave_reply);
        pthread_mutex_unlock(&game_data.mutex);
    }
}

void *advance_round(void *arg){
    while(1){
        
        for(int i=0;i<4;i++){
            if(kill(game_data.player_remote[i]->pid,0) != 0){
                game_data.player[i].in_game = false;
            }
            if(game_data.player[i].in_game){
                if(game_data.player[i].slowed){ //skip a turn
                    game_data.player[i].slowed = false;
                    continue;
                }
                point pos = game_data.player[i].position;
                point pos_original = pos;
                bool valid = false;
                if(sem_trywait(&game_data.player_remote[i]->move_request) == 0){
                    switch(game_data.player_remote[i]->move){
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
                        valid = true;
                        game_data.player[i].position = pos;
                    }
                    if(map[pos.y][pos.x] == MAP_BUSH){
                        game_data.player[i].slowed = true;
                    }
                    if(game_data.campsite.x == pos.x && game_data.campsite.y == pos.y){
                        game_data.player[i].coins_brought += game_data.player[i].coins_carried;
                        game_data.player[i].coins_carried = 0;
                    }
                    for(int j=0;j<MAX_TREASURES;j++){
                        if(game_data.treasures[j].type != TR_NONE &&
                        game_data.treasures[j].position.x == pos.x &&
                        game_data.treasures[j].position.y == pos.y){
                            map[game_data.treasures[j].position.y][game_data.treasures[j].position.x] = map_clean[game_data.treasures[j].position.y][game_data.treasures[j].position.x];
                            game_data.player[i].coins_carried += game_data.treasures[j].coins;
                            game_data.treasures[j].type = TR_NONE;
                            break;
                        }
                    }
                }
                if(!valid){
                    pos = pos_original;
                }
                game_data.player_remote[i]->position = game_data.player[i].position;
                game_data.player_remote[i]->deaths = game_data.player[i].deaths;
                game_data.player_remote[i]->coins_carried = game_data.player[i].coins_carried;
                game_data.player_remote[i]->coins_brought = game_data.player[i].coins_brought;
                game_data.player_remote[i]->round = game_data.round;
                for(int k=0;k<MAP_ROWS;k++){
                    for(int j=0;j<MAP_COLUMNS;j++){
                        game_data.player_remote[i]->map[k][j] = MAP_NONE;
                    }
                }
                for(int k=pos.y-PLAYER_SIGHT;k<=pos.y+PLAYER_SIGHT;k++){
                    for(int j=pos.x-PLAYER_SIGHT;j<=pos.x+PLAYER_SIGHT;j++){
                        if(k < 0 || j < 0 || k >= MAP_ROWS || j >= MAP_COLUMNS){
                            continue;
                        }
                        game_data.player_remote[i]->map[k][j] = map[k][j];
                        for(int player=0;player<4;player++){
                            if(i != player && game_data.player[player].in_game){
                                if(game_data.player[player].position.y == k && 
                                   game_data.player[player].position.x == j){
                                        game_data.player_remote[i]->map[k][j] = player+1;
                                   }
                            }
                        }
                        for(int beast=0;beast<MAX_BEAST;beast++){
                            if(game_data.beast[beast].in_game){
                                if(game_data.beast[beast].position.y == k && 
                                   game_data.beast[beast].position.x == j){
                                        game_data.player_remote[i]->map[k][j] = MAP_BEAST;
                                   }
                            }
                        }
                    }
                }
                sem_post(&game_data.player_remote[i]->move_reply);
            }
        }
        for(int beast=0;beast<MAX_BEAST;beast++){
            if(game_data.beast[beast].in_game){
                sem_post(&game_data.beast[beast].beast_move_request);
                sem_wait(&game_data.beast[beast].beast_move_reply);
            }
        }
       
        for(int player1=0;player1<4;player1++){
            for(int player2=0;player2<4;player2++){
                if(player1 != player2 && game_data.player[player1].in_game &&
                   game_data.player[player2].in_game){
                       if(game_data.player[player1].position.x == game_data.player[player2].position.x &&
                          game_data.player[player1].position.y == game_data.player[player2].position.y){
                                if(game_data.player[player1].coins_carried > 0){
                                    add_treasure_player(game_data.player[player1].position,game_data.player[player1].coins_carried);
                                }
                                game_data.player[player1].position = game_data.player[player1].spawn;
                                game_data.player[player1].slowed = false;
                                game_data.player[player1].deaths++;
                                game_data.player[player1].coins_carried = 0;
                          }
                }
            }
        }
        for(int player1=0;player1<4;player1++){
            for(int beast=0;beast<MAX_BEAST;beast++){
                if(game_data.player[player1].in_game && game_data.beast[beast].in_game){
                    if(game_data.player[player1].position.x == game_data.beast[beast].position.x &&
                       game_data.player[player1].position.y == game_data.beast[beast].position.y){
                            if(game_data.player[player1].coins_carried > 0){
                                add_treasure_player(game_data.player[player1].position,game_data.player[player1].coins_carried);
                            }
                            game_data.player[player1].position = game_data.player[player1].spawn;
                            game_data.player[player1].slowed = false;
                            game_data.player[player1].deaths++;
                            game_data.player[player1].coins_carried = 0;
                    }
                }
            }
        }
        usleep(250 * MS);
        game_data.round++;
        sem_post(&game_data.round_up);
    }
}


void add_beast(){
    if(beast_count<MAX_BEAST){
        if(game_data.beast[beast_count].in_game == false){
            game_data.beast[beast_count].in_game = true;
            game_data.beast[beast_count].position = get_empty_tile();
            sem_init(&game_data.beast[beast_count].beast_move_request,0,0);
            sem_init(&game_data.beast[beast_count].beast_move_reply,0,0);
            beast_id[beast_count] = beast_count;
            pthread_create(th_beast + beast_count,NULL,beast_move,&beast_id[beast_count++]); 
        }
    }
}

void *beast_move(void *arg){
    pthread_mutex_lock(&game_data.mutex);
    unsigned int i = *(unsigned int *)arg;
    pthread_mutex_unlock(&game_data.mutex);
    while(1){
        sem_wait(&game_data.beast[i].beast_move_request);
        bool moved = false;
        if(game_data.beast[i].in_game){
            point beast_pos = game_data.beast[i].position;
            for(int j=0;j<4;j++){
                if(game_data.player[j].in_game){
                    if(player_in_sight(beast_pos.x,beast_pos.y,
                                game_data.player[j].position.x,game_data.player[j].position.y)){  
                                        if(beast_pos.y > game_data.player[j].position.y){
                                            beast_pos.y--;
                                            if(map[beast_pos.y][beast_pos.x] != MAP_WALL){
                                                game_data.beast[i].position = beast_pos;
                                                moved = true;
                                            }
                                        }else if(beast_pos.y < game_data.player[j].position.y){
                                            beast_pos.y++;
                                            if(map[beast_pos.y][beast_pos.x] != MAP_WALL){
                                                game_data.beast[i].position = beast_pos;
                                                moved = true;
                                            }
                                        }
                                        if(!moved){
                                        if(beast_pos.x > game_data.player[j].position.x){
                                            beast_pos.x--;
                                            if(map[beast_pos.y][beast_pos.x] != MAP_WALL){
                                                game_data.beast[i].position = beast_pos;
                                                moved = true;
                                            }
                                        }else if(beast_pos.x < game_data.player[j].position.x){
                                            beast_pos.x++;
                                            if(map[beast_pos.y][beast_pos.x] != MAP_WALL){
                                                game_data.beast[i].position = beast_pos;
                                                moved = true;
                                            }
                                        }
                                    }
                                    
                                   
                                }
                                if(moved){
                                    break;
                                }
                }
            }
        }
        sem_post(&game_data.beast[i].beast_move_reply);
    }
}

bool player_in_sight(int x0, int y0, int x1, int y1){
    if (abs(y1 - y0) < abs(x1 - x0)){
        if (x0 > x1){
            return line_low(x1, y1, x0, y0);
        }
        else{
            return line_low(x0, y0, x1, y1);
        }
    }else{
        if(y0 > y1){
            return line_high(x1,y1,x0,y0);
        }else{
            return line_high(x0,y0,x1,y1);
        }
    }
}

bool line_low(int x0, int y0, int x1, int y1){
    int dx,dy,yi,d,y,x;
    dx = x1 - x0;
    dy = y1 - y0;
    yi = 1;
    if(dy < 0){
        yi *= -1;
        dy *= -1;
    }
    d = (2 * dy) - dx;
    y = y0;
    x = x0;
    while(x<x1){
        if(map[y][x] == MAP_WALL){
            return false;
        }
        if(d>0){
            y += yi;
            d += (2 * (dy - dx));
        }else{
            d += 2*dy;
        }
        x++;
    }
    return true;
}

bool line_high(int x0, int y0, int x1, int y1){
    int dx,dy,xi,d,y,x;
    dx = x1 - x0;
    dy = y1 - y0;
    xi = 1;
    if(dx < 0){
        xi *= -1;
        dx *= -1;
    }
    d = (2 * dx) - dy;
    y = y0;
    x = x0;
    while(y<y1){
        if(map[y][x] == MAP_WALL){
            return false;
        }
        if(d>0){
            x += xi;
            d += (2 * (dx - dy));
        }else{
            d += 2*dx;
        }
        y++;
    }
    return true;
}

void add_treasure_player(point pos, unsigned int amount){
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
        game_data.treasures[i].position = pos;
        game_data.treasures[i].type = TR_PLAYER;
        map[game_data.treasures[i].position.y][game_data.treasures[i].position.x] = TR_PLAYER;
    }
    
}