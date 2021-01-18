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
        mvprintw(max_y/2,max_x/2 - 20,"SERVER RUNNING WITH PID %d, MY PID %d",p_data->server_pid,my_pid);
        p_data->pid = my_pid;
    }else{
        mvprintw(max_y/2,max_x/2 - 10,"SERVER IS FULL");
        refresh();
        getchar();
        munmap(&p_data,sizeof(struct player_t));
        close(my_fd);
        return 0;
    }
    refresh();

    pthread_t th_keyboard;
    pthread_create(&th_keyboard,NULL,keyboard_event,NULL);

    pthread_join(th_keyboard,NULL);

    munmap(&p_data,sizeof(struct player_t));
    close(my_fd);
    endwin();
    return 0;
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
            default:
                break;
        }
        if(move){
            sem_post(&p_data->move_request);
            sem_wait(&p_data->move_reply);    
        }
    }
}