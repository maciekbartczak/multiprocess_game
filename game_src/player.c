#include "player.h"
#include <ncurses.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pthread.h>

int max_x,max_y;
struct player_t *p_data;

int main(void){
    initscr();

    noecho();
    curs_set(0);
    getmaxyx(stdscr,max_y,max_x);

    int my_fd = shm_open("player",O_RDWR,0666);
    if(my_fd == -1){
        mvprintw(max_y/2,max_x/2 - 9,"SERVER NOT RUNNING");
        refresh();
        getchar();
        return 0;
    }
    p_data = mmap(NULL,sizeof(struct player_t),PROT_READ | PROT_WRITE, MAP_SHARED,my_fd,0);
            
    if(!p_data->in_game){
        pid_t my_pid = getpid();
        sem_post(&p_data->join_request);
        sem_wait(&p_data->join_reply);
        p_data->pid = my_pid;
    }else{
        mvprintw(max_y/2,max_x/2 - 10,"SERVER IS FULL");
        refresh();
        usleep(5000 * MS);
        munmap(&p_data,sizeof(struct player_t));
        close(my_fd);
        return 0;
    }
    refresh();

    pthread_t th_keyboard,th_map;
    pthread_create(&th_keyboard,NULL,keyboard_event,NULL);
    pthread_create(&th_map,NULL,print_map,NULL);

    pthread_join(th_keyboard,NULL);

    munmap(&p_data,sizeof(struct player_t));
    close(my_fd);
    endwin();
    return 0;
}

void *print_map(void *arg){
    start_color();
    init_pair(PLAYER_PAIR,COLOR_WHITE,COLOR_MAGENTA);
    init_pair(WALL_PAIR,COLOR_CYAN,COLOR_CYAN);
    init_pair(BUSH_PAIR,COLOR_BLACK,COLOR_BLUE);
    init_pair(BEAST_PAIR,COLOR_RED,COLOR_BLUE);
    init_pair(TREASURE_PAIR,COLOR_BLACK,COLOR_YELLOW);
    init_pair(CAMPSITE_PAIR,COLOR_YELLOW,COLOR_GREEN);
    init_pair(EMPTY_PAIR,COLOR_BLUE,COLOR_BLUE);
    init_pair(NONE_PAIR,COLOR_BLACK,COLOR_BLACK);

    while(1){
        for(int i=0;i<MAP_ROWS;i++){
            for(int j=0;j<MAP_COLUMNS;j++){
                switch(p_data->map[i][j]){
                    case MAP_WALL:
                        attron(COLOR_PAIR(WALL_PAIR));
                        mvprintw(i, j,"%c",p_data->map[i][j]);
                        attroff(COLOR_PAIR(WALL_PAIR));
                        break;
                    case MAP_BUSH:
                        attron(COLOR_PAIR(BUSH_PAIR));
                        mvprintw(i, j,"%c",p_data->map[i][j]);
                        attroff(COLOR_PAIR(BUSH_PAIR));
                        break;
                    case MAP_EMPTY:
                        attron(COLOR_PAIR(EMPTY_PAIR));
                        mvprintw(i, j,"%c",p_data->map[i][j]);
                        attroff(COLOR_PAIR(EMPTY_PAIR));
                        break;
                    case MAP_TR_COIN:
                    case MAP_TR_TREASURE:
                    case MAP_TR_TREASURE_LARGE:
                    case MAP_TR_PLAYER:
                        attron(COLOR_PAIR(TREASURE_PAIR));
                        mvprintw(i,j,"%c",p_data->map[i][j]);
                        attroff(COLOR_PAIR(TREASURE_PAIR));
                        break;
                    case MAP_CAMPSITE:
                        attron(COLOR_PAIR(CAMPSITE_PAIR));
                        mvprintw(i,j,"%c",p_data->map[i][j]);
                        attroff(COLOR_PAIR(CAMPSITE_PAIR));
                        break;
                    case MAP_NONE:
                        attron(COLOR_PAIR(NONE_PAIR));
                        mvprintw(i,j,"%c",p_data->map[i][j]);
                        attroff(COLOR_PAIR(NONE_PAIR));
                    default:
                        break;
                } 
            }
        }

        attron(COLOR_PAIR(PLAYER_PAIR));
        mvprintw(p_data->position.y,p_data->position.x,"%d",2);
        attroff(COLOR_PAIR(PLAYER_PAIR));

        mvprintw(0,MAP_COLUMNS + 2,"Server PID: %d",p_data->server_pid);
        mvprintw(2,MAP_COLUMNS + 4,"Round number: %d",p_data->round);
        mvprintw(5,MAP_COLUMNS + 2,"Parameter:   Player2");
        mvprintw(6,MAP_COLUMNS + 4,"PID");
        mvprintw(7,MAP_COLUMNS + 4,"X\\Y");
        mvprintw(8,MAP_COLUMNS + 4,"Deaths");
        mvprintw(11,MAP_COLUMNS + 2,"Coins");
        mvprintw(12,MAP_COLUMNS + 4,"Carried");
        mvprintw(13,MAP_COLUMNS + 4,"Brought");
        mvprintw(6,MAP_COLUMNS + 4 + 11 ,"%d",p_data->pid);
        mvprintw(7,MAP_COLUMNS + 4 + 11 ,"%d\\%d",
                 p_data->position.x, p_data->position.y);
        mvprintw(9,MAP_COLUMNS + 4 + 11 ,"%d",p_data->deaths);
        if(p_data->coins_carried == 0){
             mvprintw(12,MAP_COLUMNS + 4 + 11,"%d   ",p_data->coins_carried);
        }else{
            mvprintw(12,MAP_COLUMNS + 4 + 11,"%d",p_data->coins_carried);
        }
        mvprintw(13,MAP_COLUMNS + 4 + 11,"%d",p_data->coins_brought);

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
        usleep(100 * MS);
    }
}

void *keyboard_event(void *arg){
    int k;
    while(1){
        bool move = false;
        k = getchar();
        switch(k){
            case 'q':
                sem_post(&p_data->leave_request);
                sem_wait(&p_data->leave_reply);
                return NULL;
            case 'w':
                p_data->move = PM_UP;
                move = true;
                break;
            case 's':
                p_data->move = PM_DOWN;
                move = true;
                break;
            case 'a':
                p_data->move = PM_LEFT;
                move = true;
                break;
            case 'd':
                p_data->move = PM_RIGHT;
                move = true;
                break;
            case 'c':
                p_data->coins_brought += 100;
                break;
            default:
                break;
        }
        if(move){
            sem_post(&p_data->move_request);
            sem_wait(&p_data->move_reply);    
        }
    }
}