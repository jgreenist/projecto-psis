#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>

//gcc teste.c UI_library.c -o teste-UI -lSDL2 -lSDL2_image

#include "com_protocol.h"
#include "UI_library.h"
#include "list_ID.h"
#include "pac_func.h"

//----------------------------------------------------------//
//----------Global variables needed-------------------------//
//----------------------------------------------------------//
IDList *Clients;
Fruits_Struct *Fruits;
int ** board;

pthread_mutex_t  mux;
pthread_rwlock_t end_mux;
int thread_alive=0;

//Only write in the begging and the only read operations//
int n_cols;
int n_lines;
int n_board_mux;
int flag_col;



//----------------------------------------------------------//
//----------------Variables to send data to SDL-------------//
//----------------------------------------------------------//

Uint32 Event_ShowCharacter;
Uint32 Event_Change2Characters;
Uint32 Event_ShownewCharacter;
Uint32 Event_Inactivity;
Uint32 Event_Disconnect;
Uint32 Event_ShowFruit;
Uint32 Event_ShownewFruits;
Uint32 Event_Clean2Fruits;

typedef struct Event_ShowCharacter_Data{
    int character,ID,x,y,x_old,y_old,r,g,b;
} Event_ShowCharacter_Data;

typedef struct Event_Change2Characters_Data{
    int character1,ID1,x1_old,y1_old,x1,y1,character2,ID2,x2,y2,r1,g1,b1,r2,g2,b2;
} Event_Change2Characters_Data;

typedef struct Event_ShownewCharacter_Data{
    int ID,xp,yp,xm,ym,r,g,b,sock_fd;
} Event_ShownewCharacter_Data;

typedef struct Event_Inactivity_Data{
    int character,ID,x,y,x_old,y_old,r,g,b;
} Event_Inactivity_Data;

typedef struct Event_Disconnect_Data{
    int xp,yp,xm,ym;
} Event_Disconnect_Data;

typedef struct Event_ShowFruit_Data{
    int fruit,x,y;
} Event_ShowFruit_Data;

typedef struct Event_ShownewFruits_Data{
    int fruit1,x1,y1,fruit2,x2,y2;
} Event_ShownewFruits_Data;

typedef struct Event_Clean2Fruits_Data{
    int fruit1,x1,y1,fruit2,x2,y2;
} Event_Clean2Fruits_Data;


//----------------------------------------------------------//
//Thread responsible for keeping the number of plays per sec//
//----------------------------------------------------------//

void * thr_forplayspersec(void * arg) {
    struct sem_nplayspersec * sem_struct_arg= (struct sem_nplayspersec *) arg;
    int sem_ending;
    while(1){
        sem_wait(&sem_struct_arg->sem_nplay1);
        sem_getvalue(&sem_struct_arg->sem_end, &sem_ending);
        if(sem_ending==1){
            break;
        }
        usleep(500000);
        sem_post(&sem_struct_arg->sem_nplay2);
    }
    sem_destroy(&sem_struct_arg->sem_nplay1);
    sem_destroy(&sem_struct_arg->sem_nplay2);
    sem_destroy(&sem_struct_arg->sem_end);
    pthread_exit(NULL);
}

//----------------------------------------------------------//
//---Thread responsible for checking the inactivity of -----//
//---------------a certain character------------------------//
//----------------------------------------------------------//

void * thr_inactivity(void * arg) {
    struct  sem_inactivity * inactivity_arg = (struct sem_inactivity *) arg;
    int sem_value,sem_ending,i,j,error;
    int msec = 0, trigger = 30000; /* 30000ms=30s */
    int elem,x,y,x_old,y_old;
    IDList * client_list;
    clock_t difference;


    SDL_Event new_event;
    Event_Inactivity_Data *event_data;
    event_data = malloc(sizeof(Event_Inactivity_Data));
    clock_t before = clock();
    while(1) {
        error=sem_getvalue(&inactivity_arg->sem_inactivity, &sem_value);
        if (sem_value == 0 && error == 0) {

            sem_getvalue(&inactivity_arg->sem_end, &sem_ending);
            if (sem_ending == 1) {
                break;
            }


            difference = clock() - before;
            msec = difference * 1000 / CLOCKS_PER_SEC;

            if (msec > trigger) {
                //set new random place
                pthread_mutex_lock(&mux);
                client_list = get_IDlist(Clients, inactivity_arg->ID);
                if (client_list != NULL) {
                    if (inactivity_arg->character == 1) {
                        x_old = client_list->xp;
                        y_old = client_list->yp;
                        board[y_old][x_old] = 0;


                    } else if (inactivity_arg->character == 3) {
                        x_old = client_list->xm;
                        y_old = client_list->ym;
                        board[y_old][x_old] = 0;
                    }
                } else {
                    sem_destroy(&inactivity_arg->sem_inactivity);
                    sem_destroy(&inactivity_arg->sem_end);
                    free(event_data);
                    pthread_exit(NULL);
                }

                elem = set_initialpos(board, n_cols, n_lines);
                x = elem % n_cols;
                y = elem / n_cols;

                //change x_ID and y_ID
                if (inactivity_arg->character == 1) {

                    client_list->xp = x;
                    client_list->yp = y;
                    if (client_list->superpower == 0) {
                        board[y][x] = inactivity_arg->ID + 10;
                    } else {
                        board[y][x] = inactivity_arg->ID + 10 + 1;
                    }

                }
                if (inactivity_arg->character == 3) {

                    client_list->xm = x;
                    client_list->ym = y;
                    board[y][x] = -(inactivity_arg->ID + 10);

                }

                event_data->character = inactivity_arg->character;
                event_data->x_old = x_old;
                event_data->y_old = y_old;
                event_data->x = x;
                event_data->y = y;
                event_data->r = inactivity_arg->rgb_r;
                event_data->g = inactivity_arg->rgb_g;
                event_data->b = inactivity_arg->rgb_b;
                event_data->ID = inactivity_arg->ID;

                SDL_zero(new_event);
                new_event.type = Event_Inactivity;
                new_event.user.data1 = event_data;
                SDL_PushEvent(&new_event);

                pthread_mutex_unlock(&mux);


                printf("new position x:%d y:%d \n", x, y);
                printf("old position x:%d y:%d \n", x_old, y_old);
                for (i = 0; i < n_lines; i++) {
                    printf("%2d ", i);
                    for (j = 0; j < n_cols; j++) {
                        printf("%3d", board[i][j]);
                    }
                    printf("\n");
                }
                //reset clock
                before = clock();
            }
            usleep(250000);

        } else if (error == -1) {
            sem_destroy(&inactivity_arg->sem_inactivity);
            sem_destroy(&inactivity_arg->sem_end);
            free(event_data);
            pthread_exit(NULL);

        } else if (sem_value == 1) {
            sem_wait(&inactivity_arg->sem_inactivity);
            //reset clock
            before = clock();
        } else {
            sem_destroy(&inactivity_arg->sem_inactivity);
            sem_destroy(&inactivity_arg->sem_end);
            free(event_data);
            pthread_exit(NULL);
        }
    }
    sem_destroy(&inactivity_arg->sem_inactivity);
    sem_destroy(&inactivity_arg->sem_end);
    free(event_data);
    pthread_exit(NULL);

    //printf("Time taken %d seconds %d milliseconds (%d iterations)\n",msec/1000, msec%1000, iterations);

}

//----------------------------------------------------------//
//----Thread responsible for placing ta cherry or a lemon---//
//----------------after two second-----------*--------------//

void * thr_placefruit(void * arg) {
    struct Fruits_Struct * my_arg = (struct Fruits_Struct *)arg;

    SDL_Event new_event;
    Event_ShowFruit_Data *event_data;
    event_data = malloc(sizeof(Event_ShowFruit_Data));
    int fruit=my_arg->fruit;
    int elem,sem_value;

    while(1){
        sem_wait(&my_arg->sem_fruit1);
        sem_post(&my_arg->sem_fruit2);


        sem_getvalue(&my_arg->end_thread_fruit, &sem_value);
        if(sem_value==1){
            break;
        }

        sleep(2);
        pthread_mutex_lock(&mux);
        elem=set_initialpos(board, n_cols,n_lines);
        board[elem/n_cols][elem%n_cols]=my_arg->fruit;
        my_arg->x=elem%n_cols;
        my_arg->y=elem/n_cols;
        event_data->x =elem%n_cols;
        event_data->y =elem/n_cols;
        event_data->fruit = fruit;


        SDL_zero(new_event);
        new_event.type = Event_ShowFruit;
        new_event.user.data1 = event_data;
        SDL_PushEvent(&new_event);
        sem_wait(&my_arg->sem_fruit2);
        pthread_mutex_unlock(&mux);
    }
    sem_destroy(&my_arg->end_thread_fruit);
    sem_destroy(&my_arg->sem_fruit1);
    sem_destroy(&my_arg->sem_fruit2);
    free(event_data);
    pthread_exit(NULL);
}
//----------------------------------------------------------//
//---------------CHECK IF A MOVE IS VALID FUNCTION----------//
//----------------------------------------------------------//

typedef struct valid_move{
    int valid; //0 or 1
    int flag2player; //0 1 (second character of other player) 2 (second character of the same player)
    int flag_newfruit; //0 or 1
    int character1,character2,new_x1,new_y1,new_x2,new_y2;
    int other_ID;
}valid_move;

//---------- no rebound move ----//
valid_move check_move_pacman_norebound(int ID,int x_old , int y_old, int x_new, int y_new){
    valid_move validity_move;
    int ID_other,elem;

    if (board[y_new][x_new] == 1){
        validity_move.character1=1;
        validity_move.new_x1 = x_old;
        validity_move.new_y1 = y_old;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;


    }else if (board[y_new][x_new] == 4){
        board[y_new][x_new] = 10 + ID+1;
        board[y_old][x_old]=0;

        validity_move.character1=1;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit = 1;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == 5){
        board[y_new][x_new] = 10 + ID+1;
        board[y_old][x_old]=0;

        validity_move.character1=1;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit = 2;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] > 10) {
        ID_other = board[y_new][x_new] - 10;
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = 10 + ID;

        validity_move.character1=1;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        if (ID_other % 2 == 0) {
            validity_move.character2 = 2;
            validity_move.other_ID=ID_other-1;

        } else {
            validity_move.character2 = 1;
            validity_move.other_ID=ID_other;
        }

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 1;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == -(ID + 10)) {
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = ID + 10;

        validity_move.character1=1;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2 = 3;
        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 2;
        validity_move.valid = 1;
        return  validity_move;


    }else if (board[y_new][x_new] <-10) {
        ID_other=(-board[y_new][x_new]) - 10;
        board[y_old][x_old] = 0;
        elem=set_initialpos(board, n_cols,n_lines);
        x_new =elem%n_cols;
        y_new =elem/n_cols;
        board[y_new][x_new] = ID + 10;



        validity_move.character1=1;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=3;
        validity_move.new_x2=x_new;
        validity_move.new_y2=y_new;

        validity_move.other_ID = ID_other;
        validity_move.flag_newfruit=0;
        validity_move.flag2player = 0;
        validity_move.valid = 1;
        return  validity_move;
    }else if (board[y_new][x_new] == 0){
        board[y_old][x_old] = 0;
        board[y_new][x_new] = ID + 10;

        validity_move.character1=1;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 0;
        validity_move.valid = 1;
        return  validity_move;
    }
    return  validity_move;

}
valid_move check_move_superpacman_norebound(int ID,int x_old , int y_old, int x_new, int y_new,int superpower) {
    valid_move validity_move;
    int ID_other,elem;

    if (board[y_new][x_new] == 1){
        validity_move.character1=2;
        validity_move.new_x1 = x_old;
        validity_move.new_y1 = y_old;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == 4){
        board[y_new][x_new] = 10 + ID+1;
        board[y_old][x_old]=0;

        validity_move.character1=2;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit = 1;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == 5){
        board[y_new][x_new] = 10 + ID+1;
        board[y_old][x_old]=0;

        validity_move.character1=2;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit = 2;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] > 10) {
        ID_other = board[y_new][x_new] - 10;
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = 10 + ID+1;

        validity_move.character1=2;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        if (ID_other % 2 == 0) {
            validity_move.character2 = 2;
            validity_move.other_ID=ID_other-1;

        } else {
            validity_move.character2 = 1;
            validity_move.other_ID=ID_other;

        }
        validity_move.flag_newfruit=0;
        validity_move.flag2player = 1;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == -(ID + 10)) {
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = ID + 10+1;

        validity_move.character1=2;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2 = 3;
        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 2;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] < -10) {
        //monster and eats it
        ID_other = (-board[y_new][x_new])-10 ;
        board[y_old][x_old] = 0;
        board[y_new][x_new] = 10 + ID+1;
        elem = set_initialpos(board, n_cols, n_lines);
        board[elem / n_cols][elem % n_cols] = -(10 + ID_other);

        validity_move.character1=2;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2 = 3;
        validity_move.new_x2 = elem % n_cols;
        validity_move.new_y2 = elem / n_cols;
        validity_move.other_ID=ID_other;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 3;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == 0){
        board[y_old][x_old] = 0;
        board[y_new][x_new] = ID + 10+1;

        validity_move.character1=2;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 0;
        validity_move.valid = 1;
        return  validity_move;
    }
    return  validity_move;
}
valid_move check_move_monster_norebound(int ID,int x_old , int y_old, int x_new, int y_new){
    valid_move validity_move;

    int ID_other,elem;

    if (board[y_new][x_new] == 1){
        validity_move.character1=3;
        validity_move.new_x1 = x_old;
        validity_move.new_y1 = y_old;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == 4){
        board[y_new][x_new] = -(10 + ID);
        board[y_old][x_old]=0;
        validity_move.character1=3;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit = 1;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;


    }else if (board[y_new][x_new] == 5){
        board[y_new][x_new] = -(10 + ID);
        board[y_old][x_old]=0;
        validity_move.character1=3;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;


        validity_move.character2=0;
        validity_move.new_x2=0;
        validity_move.new_y2=0;
        validity_move.other_ID=0;

        validity_move.flag_newfruit = 2;
        validity_move.flag2player=0;
        validity_move.valid = 1;
        return  validity_move;

    } else if (board[y_new][x_new] < -10) {
        ID_other = (-10) - (board[y_new][x_new]);
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = -(10 + ID);

        validity_move.character1=3;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2 = 3;
        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        validity_move.other_ID=ID_other;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 1;
        validity_move.valid = 1;
        return  validity_move;

        //monster of another player
    } else if (board[y_new][x_new] == (ID + 10)) {

        //the pacman of the same player
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = -(ID + 10);

        validity_move.character1=3;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;


        validity_move.character2 = 1;
        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 2;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] == (ID + 10+1)){
    //the superpacman of the same player
        board[y_old][x_old] = board[y_new][x_new];
        board[y_new][x_new] = -(ID + 10);

        validity_move.character1=3;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2 = 2;
        validity_move.new_x2 = x_old;
        validity_move.new_y2 = y_old;
        validity_move.other_ID=0;

        validity_move.flag_newfruit=0;
        validity_move.flag2player = 2;
        validity_move.valid = 1;
        return  validity_move;

    }else if (board[y_new][x_new] >10) {

        if(board[y_new][x_new]%2==0){
            ID_other = board[y_new][x_new]-10-1;
            board[y_old][x_old] = 0;
            elem=set_initialpos(board, n_cols,n_lines);
            x_new =elem%n_cols;
            y_new =elem/n_cols;
            board[y_new][x_new] = -(ID + 10);

            validity_move.character1=3;
            validity_move.new_x1 = x_new;
            validity_move.new_y1 = y_new;

            validity_move.character2 = 0;
            validity_move.new_x2 = 0;
            validity_move.new_y2 = 0;
            validity_move.other_ID=ID_other;

            //superpowerpacman
            //gets eaten
            validity_move.flag_newfruit=0;
            validity_move.flag2player = 4;
            validity_move.valid = 1;
            return  validity_move;


        }else {
            //pacman
            // eats it
            ID_other = board[y_new][x_new] - 10;
            board[y_old][x_old] = 0;
            board[y_new][x_new] = -(10 + ID);
            elem = set_initialpos(board, n_cols, n_lines);
            board[elem / n_cols][elem % n_cols] = (10 + ID_other);

            validity_move.character1 =3;
            validity_move.new_x1 = x_new;
            validity_move.new_y1 = y_new;

            validity_move.character2 = 1;
            validity_move.new_x2 = elem % n_cols;
            validity_move.new_y2 = elem / n_cols;
            validity_move.other_ID=ID_other;

            validity_move.flag_newfruit=0;
            validity_move.flag2player = 1;
            validity_move.valid = 1;
            return  validity_move;
        }
    }else if (board[y_new][x_new] == 0){
        board[y_old][x_old] = 0;
        board[y_new][x_new] = -(ID + 10);

        validity_move.character1=3;
        validity_move.new_x1 = x_new;
        validity_move.new_y1 = y_new;

        validity_move.character2 = 0;
        validity_move.new_x2 = 0;
        validity_move.new_y2 = 0;
        validity_move.other_ID=0;


        validity_move.flag_newfruit=0;
        validity_move.flag2player = 0;
        validity_move.valid = 1;
        return  validity_move;
    }
    return  validity_move;
}

valid_move check_move2( int x_new, int y_new,int x_old, int y_old, int character, int ID, int superpower) {

    valid_move validity_move;


    if (character == 1) {
        //valid

        if (x_new > (n_cols - 1)) {
            x_new = n_cols - 2;
            validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (x_new < 0) {
            x_new = 1;
            validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (y_new > (n_lines - 1)) {
            y_new = n_lines - 2;
            validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (y_new < 0) {
            y_new = 1;
            validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (x_new != x_old) {
            if (x_new > x_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (x_new - 2 >= 0) {
                        x_new = x_new - 2;
                        validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 1;
                        validity_move.new_y1 = y_new;
                        validity_move.new_x1 = 0;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else if (x_new < x_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (x_new + 2 <= n_cols - 1) {
                        x_new = x_new + 2;
                        validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);
                    } else {
                        validity_move.character1 = 1;
                        validity_move.new_y1 = y_new;
                        validity_move.new_x1 = n_cols - 1;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else {
                board[y_old][x_old] = 0;
                board[y_new][x_new] = (ID + 10);
                validity_move.character1 = 1;
                validity_move.new_y1 = y_new;
                validity_move.new_x1 = x_new;

                validity_move.character2 = 0;
                validity_move.new_x2 = 0;
                validity_move.new_y2 = 0;
                validity_move.other_ID = 0;

                validity_move.flag_newfruit = 0;
                validity_move.flag2player = 0;
                validity_move.valid = 1;
            }
        } else if (y_new != y_old) {
            if (y_new > y_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (y_new - 2 >= 0) {
                        y_new = y_new - 2;
                        validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 1;
                        validity_move.new_y1 = 0;
                        validity_move.new_x1 = x_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else if (y_new < y_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (y_new + 2 <= n_lines - 1) {
                        y_new = y_new + 2;
                        validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 1;
                        validity_move.new_y1 = n_lines - 1;
                        validity_move.new_x1 = x_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_pacman_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else {
                board[y_new][x_new] = (ID + 10);
                board[y_old][x_old] = 0;
                validity_move.character1 = 1;
                validity_move.new_y1 = y_new;
                validity_move.new_x1 = x_new;

                validity_move.character2 = 0;
                validity_move.new_x2 = 0;
                validity_move.new_y2 = 0;
                validity_move.other_ID = 0;

                validity_move.flag_newfruit = 0;
                validity_move.flag2player = 0;
                validity_move.valid = 1;
            }
        }
        return validity_move;
    }
        //-------------------------------------------------------------------------------//

        //-------------------------------------------------------------------------------//

        //-------------------------------------------------------------------------------//

    else if (character == 2) {
        //valid
        if (x_new > (n_cols - 1)) {
            x_new = n_cols - 2;
            validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

        } else if (x_new < 0) {
            x_new = 1;
            validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

        } else if (y_new > (n_lines - 1)) {
            y_new = n_lines - 2;
            validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

        } else if (y_new < 0) {
            y_new = 1;
            validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

        } else if (x_new != x_old) {
            if (x_new > x_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (x_new - 2 >= 0) {
                        x_new = x_new - 2;
                        validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

                    } else {
                        validity_move.character1 = 2;
                        validity_move.new_y1 = y_new;
                        validity_move.new_x1 = 0;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);
                }
            } else if (x_new < x_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (x_new + 2 <= n_cols - 1) {
                        x_new = x_new + 2;
                        validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

                    } else {
                        validity_move.character1 = 2;
                        validity_move.new_y1 = y_new;
                        validity_move.new_x1 = n_cols - 1;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

                }
            } else {
                board[y_old][x_old] = 0;
                board[y_new][x_new] = (ID + 10) + 1;

                validity_move.character1 = 2;
                validity_move.new_y1 = y_new;
                validity_move.new_x1 = x_new;

                validity_move.character2 = 0;
                validity_move.new_x2 = 0;
                validity_move.new_y2 = 0;
                validity_move.other_ID = 0;

                validity_move.flag_newfruit = 0;
                validity_move.flag2player = 0;
                validity_move.valid = 1;
            }
        } else if (y_new != y_old) {
            if (y_new > y_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (y_new - 2 >= 0) {
                        y_new = y_new - 2;
                        validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);
                    } else {
                        validity_move.character1 = 2;
                        validity_move.new_y1 = 0;
                        validity_move.new_x1 = x_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;

                    }
                } else {
                    validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);
                }
            } else if (y_new < y_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (y_new + 2 <= n_lines - 1) {
                        y_new = y_new + 2;
                        validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);

                    } else {
                        validity_move.character1 = 2;
                        validity_move.new_y1 = n_lines - 1;
                        validity_move.new_x1 = x_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_superpacman_norebound(ID, x_old, y_old, x_new, y_new, superpower);
                }
            } else {
                board[y_new][x_new] = (ID + 10) + 1;
                board[y_old][x_old] = 0;
                validity_move.character1 = 2;
                validity_move.new_y1 = y_new;
                validity_move.new_x1 = x_new;

                validity_move.character2 = 0;
                validity_move.new_x2 = 0;
                validity_move.new_y2 = 0;
                validity_move.other_ID = 0;

                validity_move.flag_newfruit = 0;
                validity_move.flag2player = 0;
                validity_move.valid = 1;
            }
        }
        return validity_move;
    }
        //-------------------------------------------------------------------------------//

        //-------------------------------------------------------------------------------//

        //-------------------------------------------------------------------------------//

    else if (character == 3) {

        if (x_new > (n_cols - 1)) {
            x_new = n_cols - 2;
            validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (x_new < 0) {
            x_new = 1;
            validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (y_new > (n_lines - 1)) {
            y_new = n_lines - 2;
            validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (y_new < 0) {
            y_new = 1;
            validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

        } else if (x_new != x_old) {
            if (x_new > x_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (x_new - 2 >= 0) {
                        x_new = x_new - 2;
                        validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 3;
                        validity_move.new_x1 = 0;
                        validity_move.new_y1 = y_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else if (x_new < x_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (x_new + 2 <= n_cols - 1) {
                        x_new = x_new + 2;
                        validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 3;
                        validity_move.new_x1 = n_cols - 1;
                        validity_move.new_y1 = y_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else {
                board[y_old][x_old] = 0;
                board[y_new][x_new] = -(ID + 10);
                validity_move.character1 = 3;
                validity_move.new_y1 = y_new;
                validity_move.new_x1 = x_new;

                validity_move.character2 = 0;
                validity_move.new_x2 = 0;
                validity_move.new_y2 = 0;
                validity_move.other_ID = 0;

                validity_move.flag_newfruit = 0;
                validity_move.flag2player = 0;
                validity_move.valid = 1;
            }
        } else if (y_new != y_old) {
            if (y_new > y_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (y_new - 2 >= 0) {
                        y_new = y_new - 2;
                        validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 3;
                        validity_move.new_y1 = 0;
                        validity_move.new_x1 = x_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else if (y_new < y_old && board[y_new][x_new] != 0) {
                if (board[y_new][x_new] == 1) {
                    if (y_new + 2 <= n_lines - 1) {
                        y_new = y_new + 2;
                        validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);

                    } else {
                        validity_move.character1 = 3;
                        validity_move.new_y1 = n_lines - 1;
                        validity_move.new_x1 = x_new;

                        validity_move.character2 = 0;
                        validity_move.new_x2 = 0;
                        validity_move.new_y2 = 0;
                        validity_move.other_ID = 0;

                        validity_move.flag_newfruit = 0;
                        validity_move.flag2player = 0;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move = check_move_monster_norebound(ID, x_old, y_old, x_new, y_new);
                }
            } else {
                board[y_new][x_new] = -(ID + 10);
                board[y_old][x_old] = 0;
                validity_move.character1 = 3;
                validity_move.new_y1 = y_new;
                validity_move.new_x1 = x_new;

                validity_move.character2 = 0;
                validity_move.new_x2 = 0;
                validity_move.new_y2 = 0;
                validity_move.other_ID = 0;

                validity_move.flag_newfruit = 0;
                validity_move.flag2player = 0;
                validity_move.valid = 1;
            }
        }
        return validity_move;
    }

    validity_move.character1 = 0;
    validity_move.new_y1 = 0;
    validity_move.new_x1 = 0;

    validity_move.character2 = 0;
    validity_move.new_x2 = 0;
    validity_move.new_y2 = 0;
    validity_move.other_ID = 0;

    validity_move.flag_newfruit = 0;
    validity_move.flag2player = 0;
    validity_move.valid = 0;
    return validity_move;
}


void * clientThreadi(void * arg){
    struct threadclient_arg *my_arg = (struct threadclient_arg *)arg;
    int sock_fd=(*my_arg).sock_fd;
    int ID=(*my_arg).ID;
    int i,j;

    pthread_rwlock_wrlock(&end_mux);
    thread_alive++;
    pthread_rwlock_unlock(&end_mux);

    //----------------------------------------------------------------------------------------------------------//
    //------------------------------------receiving the colour of the character---------------------------------//
    //----------------------------------------------------------------------------------------------------------//
    message m;
    int err_rcv;
    err_rcv = recv(sock_fd, &m , sizeof(m), 0);
    //---------------------------------------------------------------//
    //----------------------check message ---------------------------//
    //---------------------------------------------------------------//
    if (err_rcv<0){
        perror("Error in reading: ");
        close(sock_fd);
        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }

    if(m.action!=1){
        printf("invalid action\n");
        close(sock_fd);

        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }if(m.size!= sizeof(colour_msg)){
        printf("invalid size of message\n");
        close(sock_fd);

        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }
    //---------------------------------------------------------------//
    //---------------------------------------------------------------//


    colour_msg msg_colour;
    err_rcv = recv(sock_fd, &msg_colour , m.size, 0);
    //---------------------------------------------------------------//
    //----------------------check message ---------------------------//
    //---------------------------------------------------------------//
    if (err_rcv<0){
        perror("Error in reading: ");
        close(sock_fd);

        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }
    if(msg_colour.r<0 || msg_colour.g<0 || msg_colour.b<0 ){
        printf("invalid colour code\n");
        close(sock_fd);

        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
    }
    //---------------------------------------------------------------//
    //---------------------------------------------------------------//
    int rgb_r=msg_colour.r;
    int rgb_g=msg_colour.g;
    int rgb_b=msg_colour.b;
    int rgb_r_other,rgb_g_other,rgb_b_other;
    int x_other,y_other;


    //----------------------------------------------------------------------------------------------------------//
    //------------------------------sending board size and sending initial position-----------------------------//
    //----------------------------------------------------------------------------------------------------------//

    initialinfo_msg con_msg;
    con_msg.n_lin=n_lines;
    con_msg.n_col=n_cols;
    con_msg.ID=ID;


    pthread_mutex_lock(&mux);
    //sync needed dont use board directly
    int elem;
    int elem1=set_initialpos(board, n_cols,n_lines);
    if(elem1<0){
        printf("board is full \n");
        pthread_mutex_unlock(&mux);

        close(sock_fd);

        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
        //close thread and etc
    }
    con_msg.xpac_ini=elem1%n_cols;
    con_msg.ypac_ini=elem1/n_cols;
    board[elem1/n_cols][elem1%n_cols]=ID+10;

    int elem2=set_initialpos(board, n_cols,n_lines);
    if(elem2<0){
        printf("board is full \n");
        board[elem1/n_cols][elem1%n_cols]=0;
        pthread_mutex_unlock(&mux);

        close(sock_fd);

        pthread_rwlock_wrlock(&end_mux);
        thread_alive--;
        pthread_rwlock_unlock(&end_mux);

        pthread_detach(pthread_self());
        pthread_exit(NULL);
        //close thread and etc
    }

    con_msg.xm_ini=elem2%n_cols;
    con_msg.ym_ini=elem2/n_cols;
    board[elem2/n_cols][elem2%n_cols]=-(10+ID);
    printf("initial pos, pacman: x %d,y%d monster: x %d,y%d \n",con_msg.xpac_ini,con_msg.ypac_ini,con_msg.xm_ini,con_msg.ym_ini);

    //sending board
    send_board = (int *)malloc(sizeof(int)* (n_cols)*(n_lines));
    for(i=0;i<n_lines;i++) {
        for (j = 0; j < n_cols; j++) {
            send_board[i*(n_cols)+j]=board[i][j];
        }
    }

    //sending clients
    IDList * send_list  = (IDList *) malloc(sizeof(IDList));
    send_list=Clients;

    int size=0;
    while(send_list!=NULL){
        size++;
        send_list=get_next_ID(send_list);
    }
    con_msg.n_entries_of_client_array=size;
    send_list_array = (int *)malloc(sizeof(int)*size*4);

    send_list=Clients;

    i=0;
    while(send_list!=NULL){
        send_list_array[i*4]=send_list->ID;
        send_list_array[i*4+1]=send_list->rgb_r;
        send_list_array[i*4+2]=send_list->rgb_g;
        send_list_array[i*4+3]=send_list->rgb_b;
        i++;
        send_list=send_list->next;
    }

    m.action=10;
    m.size= sizeof(con_msg);
    err_rcv=send(sock_fd, &m, sizeof(message), 0);
    printf("sended data size:%d\n",err_rcv);
    err_rcv=send(sock_fd, &con_msg, m.size, 0);
    printf("sending initial informations completed\n");
    printf("sended data size:%d\n",err_rcv);

    m.action=11;
    m.size= sizeof(int *)*(n_cols)*(n_lines);
    err_rcv=send(sock_fd, &m, sizeof(message), 0);
    printf("sended data size:%d\n",err_rcv);
    err_rcv=send(sock_fd, send_board, m.size, 0);
    printf("sending initial informations completed\n");
    printf("sended data size:%d\n",err_rcv);

    m.action=12;
    m.size= sizeof(int)*size*4;
    err_rcv=send(sock_fd, &m, sizeof(message), 0);
    printf("sended data size:%d\n",err_rcv);
    err_rcv=send(sock_fd, send_list_array, m.size, 0);
    printf("sending initial informations completed\n");
    printf("sended data size:%d\n",err_rcv);




    //------------------------------------------------------------------------------------------------------//
    //-------------------------paint and send initial positions to all other clients------------------------//
    //------------------------------------------------------------------------------------------------------//
    SDL_Event new_event;
    Event_ShownewCharacter_Data *event_data_ini;

    event_data_ini = malloc(sizeof(Event_ShownewCharacter_Data));
    event_data_ini->xp = con_msg.xpac_ini;
    event_data_ini->yp = con_msg.ypac_ini;
    event_data_ini->xm = con_msg.xm_ini;
    event_data_ini->ym = con_msg.ym_ini;
    event_data_ini->r = rgb_r;
    event_data_ini->g = rgb_g;
    event_data_ini->b = rgb_b;

    SDL_zero(new_event);
    new_event.type = Event_ShownewCharacter;
    new_event.user.data1 = event_data_ini;
    SDL_PushEvent(&new_event);
    pthread_mutex_unlock(&mux);



    //----------------------------------------------------------------------------------------------------------//
    //---------------------------------------------Insert new client info---------------------------------------//
    //----------------------------------------------------------------------------------------------------------//
    IDList * this_client = (IDList *) malloc(sizeof(IDList));
    if(this_client == NULL){
        return NULL;
    }
    this_client->ID = ID;
    this_client->score=0;
    this_client->socket=sock_fd;
    this_client->rgb_r = rgb_r;
    this_client->rgb_g = rgb_g;
    this_client->rgb_b = rgb_b;
    this_client->xp=con_msg.xpac_ini;
    this_client->yp=con_msg.ypac_ini;
    this_client->xm=con_msg.xm_ini;
    this_client->ym=con_msg.ym_ini;
    this_client->superpower=0;
    this_client->next = NULL;


    //---------------------------------------------------------------------------------------------------------//
    //----------------------------------------Insert new fruit to the board-------------------------------------//
    //---------------------------------------------------------------------------------------------------------//
    int error;
    Fruits_Struct * Cherry_list= (Fruits_Struct *) malloc(sizeof(Fruits_Struct));
    Fruits_Struct * Lemon_list=  (Fruits_Struct *) malloc(sizeof(Fruits_Struct));
    Event_ShownewFruits_Data *event_data_newfruit;
    event_data_newfruit = malloc(sizeof(Event_ShownewFruits_Data));

    pthread_mutex_lock(&mux);
    if(Clients!=NULL){



        pthread_t thr_fruit1,thr_fruit2;
        event_data_newfruit->fruit1=0;
        event_data_newfruit->fruit2=0;


        elem=set_initialpos(board, n_cols,n_lines);
        if(elem<0){
            printf("board is full \n");
            event_data_newfruit->x1 =0;
            event_data_newfruit->y1 =0;
            event_data_newfruit->fruit1=0;
        }else{

            board[elem/n_cols][elem%n_cols]=4;

            event_data_newfruit->x1 =elem%n_cols;
            event_data_newfruit->y1 =elem/n_cols;
            event_data_newfruit->fruit1=4;

            Cherry_list->fruit=4;
            Cherry_list->x=elem%n_cols;
            Cherry_list->y=elem/n_cols;
            Cherry_list->next = NULL;

            Fruits=insert_new_Fruit(Fruits,Cherry_list);

            sem_init(&Cherry_list->sem_fruit1,0,0);
            sem_init(&Cherry_list->sem_fruit2,0,0);
            sem_init(&Cherry_list->end_thread_fruit,0,0);

            error = pthread_create(&thr_fruit1,NULL,thr_placefruit, Cherry_list);
            Cherry_list->thread_id=thr_fruit1;
        }


        elem=set_initialpos(board, n_cols,n_lines);
        if(elem<0){
            printf("board is full \n");
            event_data_newfruit->x2=0;
            event_data_newfruit->y2=0;
            event_data_newfruit->fruit2=0;
        }else {
            //SYNC here

            board[elem/n_cols][elem%n_cols]=5;

            event_data_newfruit->x2=elem%n_cols;
            event_data_newfruit->y2=elem/n_cols;
            event_data_newfruit->fruit2=5;

            //SYNC here
            //add fruit lists
            Lemon_list->fruit=5;
            Lemon_list->x=elem%n_cols;
            Lemon_list->y=elem/n_cols;
            Lemon_list->next = NULL;


            sem_init(&Lemon_list->sem_fruit1,0,0);
            sem_init(&Lemon_list->sem_fruit2,0,0);
            sem_init(&Lemon_list->end_thread_fruit,0,0);

            Fruits=insert_new_Fruit(Fruits,Lemon_list );

            error = pthread_create(&thr_fruit2,NULL,thr_placefruit, Lemon_list);
            Lemon_list->thread_id=thr_fruit1;

        }

        SDL_zero(new_event);
        new_event.type = Event_ShownewFruits;
        new_event.user.data1 = event_data_newfruit;
        SDL_PushEvent(&new_event);


    }
    Clients=insert_new_ID(Clients, this_client);
    pthread_mutex_unlock(&mux);

    //need to make sync here



    //-------------------------------------------------------//
    //------Initialize the variables for the game moves------//
    //-------------------------------------------------------//
    int x_new,y_new,character,flag_2player=0;
    send_character_pos_msg msg_pos;


    Event_ShowCharacter_Data *event_data;
    event_data = malloc(sizeof(Event_ShowCharacter_Data));

    Event_ShowCharacter_Data *event_data2;
    event_data2 = malloc(sizeof(Event_ShowCharacter_Data));

    Event_Change2Characters_Data *event_data2player;
    event_data2player = malloc(sizeof(Event_Change2Characters_Data));

    Event_Disconnect_Data *event_data_final;
    event_data_final = malloc(sizeof(Event_Disconnect_Data));

    Event_Clean2Fruits_Data *event_data_cleanfruit;
    event_data_cleanfruit = malloc(sizeof(Event_Clean2Fruits_Data));

    valid_move validity_move;
    //-------------------------------------------------------//
    //---------initiliaze sync variables for-------- --------//
    //-------------------------------------------------------//

    struct sem_nplayspersec arg_sem1;
    int sem_value_nplaypersec;
    pthread_t thr_nplayspersec1;
    sem_init(&arg_sem1.sem_nplay1, 0, 0);
    sem_init(&arg_sem1.sem_nplay2, 0, 1);
    sem_init(&arg_sem1.sem_end,0,0);
    error = pthread_create(&thr_nplayspersec1,NULL,thr_forplayspersec , &arg_sem1);

    struct sem_nplayspersec arg_sem3;
    pthread_t thr_nplayspersec3;
    sem_init(&arg_sem3.sem_nplay1, 0, 0);
    sem_init(&arg_sem3.sem_nplay2, 0, 1);
    sem_init(&arg_sem3.sem_end,0,0);
    error = pthread_create(&thr_nplayspersec3,NULL,thr_forplayspersec , &arg_sem3);

    pthread_t thr_ina1;
    struct sem_inactivity inactivity1;
    sem_init(&inactivity1.sem_inactivity,0,0);
    sem_init(&inactivity1.sem_end,0,0);
    inactivity1.ID=ID;
    inactivity1.character=1;
    inactivity1.rgb_r=rgb_r;
    inactivity1.rgb_g=rgb_g;
    inactivity1.rgb_b=rgb_b;
    error = pthread_create(&thr_ina1,NULL,thr_inactivity, &inactivity1);

    pthread_t thr_ina3;
    struct sem_inactivity inactivity3;
    sem_init(&inactivity3.sem_inactivity,0,0);
    sem_init(&inactivity3.sem_end,0,0);

    inactivity3.ID=ID;
    inactivity3.character=3;
    inactivity3.rgb_r=rgb_r;
    inactivity3.rgb_g=rgb_g;
    inactivity3.rgb_b=rgb_b;
    error = pthread_create(&thr_ina3,NULL,thr_inactivity, &inactivity3);

    int x_old;
    int y_old;
    int superpower=0;
    IDList * other_client;
    int ID_other;

    //-------------------------------------------------------//
    //---------read the moves made by the client-------------//
    //----loop receiving updated positions  from socket------//
    //-------------------------------------------------------//

    while((err_rcv=recv(sock_fd, &m, sizeof(m),0))>0){
        //---------------------------------------------------------------//
        //----------------------check message ---------------------------//
        //---------------------------------------------------------------//

        if(m.action<2 || m.action>9){
            printf("invalid action\n");
            break;
        }
        if(m.size!= sizeof(msg_pos)){
            printf("invalid message size\n");
            break;
        }

        //---------------------------------------------------------------//
        //---------------------------------------------------------------//
        if(m.action==9){
            //NEED SYNC HERE
            pthread_mutex_lock(&mux);
            board[this_client->yp][this_client->xp]=0;
            board[this_client->ym][this_client->xm]=0;

            event_data_final->xm= this_client->xm;
            event_data_final->ym = this_client->ym;
            event_data_final->xp = this_client->xp;
            event_data_final->yp = this_client->yp;
            //clean this client of client list

            Clients =remove_ID(Clients,this_client);

            //clean this client of the board representation


            //--------------needs to be done---------------//
            //Clean two fuits of the board if Clients!=NULL
            if(Clients!=NULL) {
                event_data_cleanfruit->fruit1=0;
                event_data_cleanfruit->fruit2=0;

                if(Clients->next==NULL){
                    Clients->score=0;
                    printf("Client ID: %d score: %d \n\n", Clients->ID, Clients->score);
                }

                Cherry_list = get_fruit_list(Fruits, 4);
                if(Cherry_list!=NULL){
                    board[Cherry_list->y][Cherry_list->x] = 0;

                    event_data_cleanfruit->x1=Cherry_list->x;
                    event_data_cleanfruit->y1=Cherry_list->y;
                    event_data_cleanfruit->fruit1=4;
                    sem_post(&Cherry_list->end_thread_fruit);
                    sem_post(&Cherry_list->sem_fruit1);
                    pthread_join(Cherry_list->thread_id,NULL);
                    Fruits =remove_fruit_list(Fruits, Cherry_list);
                }

                Lemon_list = get_fruit_list(Fruits, 5);
                if(Lemon_list!=NULL){
                    board[Lemon_list->y][Lemon_list->x] = 0;

                    event_data_cleanfruit->x2=Lemon_list->x;
                    event_data_cleanfruit->y2=Lemon_list->y;
                    event_data_cleanfruit->fruit2=5;
                    sem_post(&Lemon_list->end_thread_fruit);
                    sem_post(&Lemon_list->sem_fruit1);
                    pthread_join(Lemon_list->thread_id,NULL);
                    Fruits =remove_fruit_list(Fruits, Lemon_list);
                }
                //NEED SYNC HERE and free

                SDL_zero(new_event);
                new_event.type = Event_Clean2Fruits;
                new_event.user.data1 = event_data_cleanfruit;
                SDL_PushEvent(&new_event);
                //NEED SYNC HERE and free


            }

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_Disconnect;
            new_event.user.data1 = event_data_final;
            SDL_PushEvent(&new_event);
            pthread_mutex_unlock(&mux);

            sem_post(&inactivity1.sem_end);
            sem_post(&inactivity3.sem_end);

            sem_post(&arg_sem1.sem_nplay1);
            sem_post(&arg_sem1.sem_end);

            sem_post(&arg_sem3.sem_nplay1);
            sem_post(&arg_sem3.sem_end);



            //clear semaphores
            pthread_join(thr_ina1, NULL);
            pthread_join(thr_ina3, NULL);
            pthread_join(thr_nplayspersec1, NULL);
            pthread_join(thr_nplayspersec3, NULL);
            //clear socket
            close(sock_fd);
            free(event_data);
            free(event_data_newfruit);
            free(event_data2player);
            free(event_data_cleanfruit);
            free(event_data_ini);
            free(event_data_final);

            printf("%d %d\n", n_cols, n_lines);
            for ( i = 0 ; i < n_lines; i++){
                printf("%2d ", i);
                for ( j = 0 ; j < n_cols; j++) {
                    printf("%d", board[i][j]);
                }
                printf("\n");
            }

            pthread_rwlock_wrlock(&end_mux);
            thread_alive--;
            pthread_rwlock_unlock(&end_mux);

            pthread_detach(pthread_self());
            pthread_exit(NULL);
        }
        //---------------------------------------------------------------//
        //----------------------check message ---------------------------//
        //---------------------------------------------------------------//

        err_rcv = recv(sock_fd, &msg_pos, sizeof(msg_pos), 0);
        if(msg_pos.ID!=ID){
            printf("invalid action\n");
            break;
        }
        if(msg_pos.character==1 || msg_pos.character==3){
        }else{
            printf("invalid character\n");
            continue;
        }
        if(msg_pos.x<-1 || msg_pos.x>n_cols || msg_pos.y<-1|| msg_pos.y>n_lines){
            printf("outside of board\n");
            continue;
        }

        //---------------------------------------------------------------//
        //---------------------------------------------------------------//


        x_new = msg_pos.x;
        y_new = msg_pos.y;
        character = msg_pos.character;

        // get old position NEED SYNC

        pthread_mutex_lock(&mux);
        if (character == 1) {
            x_old = this_client->xp;
            y_old = this_client->yp;
            superpower=this_client->superpower;
        } else if (character == 3) {

            x_old = this_client->xm;
            y_old = this_client->ym;
            superpower=this_client->superpower;
        } else {
            pthread_mutex_unlock(&mux);
            continue;
        }

        if (x_new == x_old && y_new == y_old) {
            pthread_mutex_unlock(&mux);
            continue;
        }else if ((((x_new - 1 == x_old || x_new + 1 == x_old) && y_new == y_old) ||((y_new - 1 == y_old || y_new + 1 == y_old) && x_new == x_old))==0) {
            pthread_mutex_unlock(&mux);
            continue;
        }else{
            if(character==1){
                sem_getvalue(&arg_sem1.sem_nplay2, &sem_value_nplaypersec);
                if (sem_value_nplaypersec == 0) {
                    pthread_mutex_unlock(&mux);
                    continue;
                } else {

                    if(superpower>0){
                        character=2;
                    }
                    validity_move = check_move2( x_new, y_new, x_old, y_old, character, ID, superpower);


                    if(validity_move.valid == 1 && validity_move.flag_newfruit == 1){

                        this_client->superpower=1;
                        this_client->xp=validity_move.new_x1;
                        this_client->yp=validity_move.new_y1;
                        this_client->score++;


                        Cherry_list = get_fruit_list(Fruits,4);
                        sem_post(&Cherry_list->sem_fruit1);

                        event_data->ID=ID;
                        event_data->character = 2;
                        event_data->x = validity_move.new_x1;
                        event_data->y = validity_move.new_y1;
                        event_data->x_old = x_old;
                        event_data->y_old = y_old;
                        event_data->r = rgb_r;
                        event_data->g = rgb_g;
                        event_data->b = rgb_b;
                        event_data->ID=ID;

                        // send the event
                        SDL_zero(new_event);
                        new_event.type = Event_ShowCharacter;
                        new_event.user.data1 = event_data;
                        SDL_PushEvent(&new_event);



                        sem_post(&inactivity1.sem_inactivity);
                        sem_wait(&arg_sem1.sem_nplay2);
                        sem_post(&arg_sem1.sem_nplay1);

                    }else if(validity_move.valid == 1 && validity_move.flag_newfruit == 2){
                        this_client->score++;
                        this_client->xp=validity_move.new_x1;
                        this_client->yp=validity_move.new_y1;
                        this_client->superpower=1;


                        Lemon_list = get_fruit_list(Fruits,5);
                        sem_post(&Lemon_list->sem_fruit1);

                        event_data->ID=ID;
                        event_data->character = 2;
                        event_data->x = validity_move.new_x1;
                        event_data->y = validity_move.new_y1;
                        event_data->x_old = x_old;
                        event_data->y_old = y_old;
                        event_data->r = rgb_r;
                        event_data->g = rgb_g;
                        event_data->b = rgb_b;
                        event_data->ID=ID;


                        // send the event
                        SDL_zero(new_event);
                        new_event.type = Event_ShowCharacter;
                        new_event.user.data1 = event_data;
                        SDL_PushEvent(&new_event);


                        sem_post(&inactivity1.sem_inactivity);
                        sem_wait(&arg_sem1.sem_nplay2);
                        sem_post(&arg_sem1.sem_nplay1);

                    }else if (validity_move.valid == 1) {

                        flag_2player = validity_move.flag2player;

                        this_client->xp=validity_move.new_x1;
                        this_client->yp=validity_move.new_y1;

                        if (flag_2player == 0) {

                            if(validity_move.character2==3) {
                                ID_other = validity_move.other_ID;
                                other_client = get_IDlist(Clients, ID_other);
                                other_client->score++;

                            }
                            event_data->character = validity_move.character1;
                            event_data->x = validity_move.new_x1;
                            event_data->y = validity_move.new_y1;
                            event_data->x_old = x_old;
                            event_data->y_old = y_old;
                            event_data->r = rgb_r;
                            event_data->g = rgb_g;
                            event_data->b = rgb_b;
                            event_data->ID=ID;


                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_ShowCharacter;
                            new_event.user.data1 = event_data;
                            SDL_PushEvent(&new_event);

                        }else if (flag_2player == 1 || flag_2player == 3) {
                            if(flag_2player==3){

                                // superpacman eats a monster
                                this_client->superpower++;
                                if (this_client->superpower>=3){
                                    this_client->superpower=0;
                                    validity_move.character1=1;
                                }
                                this_client->score++;

                            }

                            ID_other = validity_move.other_ID;

                            other_client = get_IDlist(Clients, ID_other);
                            if(validity_move.character2==1 || validity_move.character2==2 ){
                                other_client->xp=validity_move.new_x2;
                                other_client->yp=validity_move.new_y2;

                            }
                            if(validity_move.character2==3){
                                other_client->xm=validity_move.new_x2;
                                other_client->ym=validity_move.new_y2;
                            }


                            rgb_r_other=other_client->rgb_r;
                            rgb_g_other=other_client->rgb_g;
                            rgb_b_other=other_client->rgb_b;


                            event_data2player->character1 = validity_move.character1;
                            event_data2player->x1 = validity_move.new_x1;
                            event_data2player->y1 = validity_move.new_y1;
                            event_data2player->x1_old = x_old;
                            event_data2player->y1_old = y_old;
                            event_data2player->character2 = validity_move.character2;
                            event_data2player->x2 = validity_move.new_x2;
                            event_data2player->y2 = validity_move.new_y2;
                            event_data2player->r1 = rgb_r;
                            event_data2player->g1 = rgb_g;
                            event_data2player->b1 = rgb_b;
                            event_data2player->r2 = rgb_r_other;
                            event_data2player->g2 = rgb_g_other;
                            event_data2player->b2 = rgb_b_other;
                            event_data2player->ID1=ID;
                            event_data2player->ID2=validity_move.other_ID;

                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_Change2Characters;
                            new_event.user.data1 = event_data2player;
                            SDL_PushEvent(&new_event);
                            flag_2player = 0;


                        }else if (flag_2player == 2) {
                            this_client->xp=validity_move.new_x1;
                            this_client->yp=validity_move.new_y1;
                            this_client->xm=validity_move.new_x2;
                            this_client->ym=validity_move.new_y2;



                            event_data2player->character1 = validity_move.character1;
                            event_data2player->x1 = validity_move.new_x1;
                            event_data2player->y1 = validity_move.new_y1;
                            event_data2player->x1_old = x_old;
                            event_data2player->y1_old = y_old;
                            event_data2player->character2 = validity_move.character2;
                            event_data2player->x2 = validity_move.new_x2;
                            event_data2player->y2 = validity_move.new_y2;
                            event_data2player->r1 = rgb_r;
                            event_data2player->g1 = rgb_g;
                            event_data2player->b1 = rgb_b;
                            event_data2player->r2 = rgb_r;
                            event_data2player->g2 = rgb_g;
                            event_data2player->b2 = rgb_b;
                            event_data2player->ID1=ID;
                            event_data2player->ID2=ID;

                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_Change2Characters;
                            new_event.user.data1 = event_data2player;
                            SDL_PushEvent(&new_event);

                            flag_2player = 0;
                        }


                        for (i = 0; i < n_lines; i++) {
                            printf("%2d ", i);
                            for (j = 0; j < n_cols; j++) {
                                printf("%3d", board[i][j]);
                            }
                            printf("\n");
                        }
                        sem_post(&inactivity1.sem_inactivity);
                        sem_wait(&arg_sem1.sem_nplay2);
                        sem_post(&arg_sem1.sem_nplay1);
                    }
                }

            }else if(character==3){
                sem_getvalue(&arg_sem3.sem_nplay2, &sem_value_nplaypersec);
                if (sem_value_nplaypersec == 0) {
                    pthread_mutex_unlock(&mux);

                    continue;
                } else {
                    validity_move = check_move2( x_new, y_new, x_old, y_old, character, ID,superpower);


                    if(validity_move.valid == 1 && validity_move.flag_newfruit == 1){
                        Cherry_list = get_fruit_list(Fruits,4);
                        sem_post(&Cherry_list->sem_fruit1);

                        this_client->score++;
                        this_client->xm= validity_move.new_x1;
                        this_client->ym= validity_move.new_y1;

                        event_data->ID=ID;
                        event_data->character = validity_move.character1;
                        event_data->x = validity_move.new_x1;
                        event_data->y = validity_move.new_y1;
                        event_data->x_old = x_old;
                        event_data->y_old = y_old;
                        event_data->r = rgb_r;
                        event_data->g = rgb_g;
                        event_data->b = rgb_b;

                        // send the event
                        SDL_zero(new_event);
                        new_event.type = Event_ShowCharacter;
                        new_event.user.data1 = event_data;
                        SDL_PushEvent(&new_event);


                        sem_post(&inactivity3.sem_inactivity);
                        sem_wait(&arg_sem3.sem_nplay2);
                        sem_post(&arg_sem3.sem_nplay1);

                    }else if(validity_move.valid == 1 && validity_move.flag_newfruit == 2){
                        Lemon_list = get_fruit_list(Fruits,5);
                        sem_post(&Lemon_list->sem_fruit1);

                        this_client->score++;
                        this_client->xm= validity_move.new_x1;
                        this_client->ym= validity_move.new_y1;

                        event_data->ID=ID;
                        event_data->character = validity_move.character1;
                        event_data->x = validity_move.new_x1;
                        event_data->y = validity_move.new_y1;
                        event_data->x_old = x_old;
                        event_data->y_old = y_old;
                        event_data->r = rgb_r;
                        event_data->g = rgb_g;
                        event_data->b = rgb_b;

                        // send the event
                        SDL_zero(new_event);
                        new_event.type = Event_ShowCharacter;
                        new_event.user.data1 = event_data;
                        SDL_PushEvent(&new_event);


                        sem_post(&inactivity3.sem_inactivity);
                        sem_wait(&arg_sem3.sem_nplay2);
                        sem_post(&arg_sem3.sem_nplay1);

                    }else if (validity_move.valid == 1) {
                        flag_2player = validity_move.flag2player;
                        if (flag_2player == 0 || flag_2player == 4) {
                            if(flag_2player==4){
                                //encounters superpowerpacman and gets eaten

                                ID_other = validity_move.other_ID;
                                other_client = get_IDlist(Clients, ID_other);
                                other_client->score++;
                                other_client->superpower++;
                                x_other=other_client->xp;
                                y_other=other_client->yp;
                                rgb_r_other=other_client->rgb_r;
                                rgb_g_other=other_client->rgb_g;
                                rgb_b_other=other_client->rgb_b;

                                if (other_client->superpower>=3){
                                    other_client->superpower=0;

                                    board[y_other][x_other]=board[y_other][x_other]-1;

                                    event_data2->character = 1;
                                    event_data2->x = x_other;
                                    event_data2->y = y_other;
                                    event_data2->x_old = x_other;
                                    event_data2->y_old = y_other;
                                    event_data2->r = rgb_r_other;
                                    event_data2->g = rgb_g_other;
                                    event_data2->b = rgb_b_other;
                                    event_data2->ID=ID_other;

                                    // send the event
                                    SDL_zero(new_event);
                                    new_event.type = Event_ShowCharacter;
                                    new_event.user.data1 = event_data2;
                                    SDL_PushEvent(&new_event);
                                }else{
                                }
                            }

                            this_client->xm=validity_move.new_x1;
                            this_client->ym=validity_move.new_y1;


                            event_data->character = validity_move.character1;
                            event_data->x = validity_move.new_x1;
                            event_data->y = validity_move.new_y1;
                            event_data->x_old = x_old;
                            event_data->y_old = y_old;
                            event_data->r = rgb_r;
                            event_data->g = rgb_g;
                            event_data->b = rgb_b;
                            event_data->ID=ID;


                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_ShowCharacter;
                            new_event.user.data1 = event_data;
                            SDL_PushEvent(&new_event);


                        }
                        if (flag_2player == 1) {


                            this_client->xm=validity_move.new_x1;
                            this_client->ym=validity_move.new_y1;

                            if(validity_move.character2==3){
                                ID_other = validity_move.other_ID;
                                other_client = get_IDlist(Clients, ID_other);
                                other_client->xm=validity_move.new_x2;
                                other_client->ym=validity_move.new_y2;
                                rgb_r_other=other_client->rgb_r;
                                rgb_g_other=other_client->rgb_g;
                                rgb_b_other=other_client->rgb_b;


                            }else if(validity_move.character2==1){
                                ID_other = validity_move.other_ID;
                                other_client = get_IDlist(Clients, ID_other);
                                other_client->xp=validity_move.new_x2;
                                other_client->yp=validity_move.new_y2;
                                this_client->score++;
                                rgb_r_other=other_client->rgb_r;
                                rgb_g_other=other_client->rgb_g;
                                rgb_b_other=other_client->rgb_b;

                            }else{

                            }


                            event_data2player->character1 = validity_move.character1;
                            event_data2player->x1 = validity_move.new_x1;
                            event_data2player->y1 = validity_move.new_y1;
                            event_data2player->x1_old = x_old;
                            event_data2player->y1_old = y_old;
                            event_data2player->character2 = validity_move.character2;
                            event_data2player->x2 = validity_move.new_x2;
                            event_data2player->y2 = validity_move.new_y2;
                            event_data2player->r1 = rgb_r;
                            event_data2player->g1 = rgb_g;
                            event_data2player->b1 = rgb_b;
                            event_data2player->r2 = rgb_r_other;
                            event_data2player->g2 = rgb_g_other;
                            event_data2player->b2 = rgb_b_other;
                            event_data2player->ID1=ID;
                            event_data2player->ID2=validity_move.other_ID;

                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_Change2Characters;
                            new_event.user.data1 = event_data2player;
                            SDL_PushEvent(&new_event);
                            flag_2player = 0;
                        }
                        if (flag_2player == 2){

                            this_client->xm=validity_move.new_x1;
                            this_client->ym=validity_move.new_y1;
                            this_client->xp=validity_move.new_x2;
                            this_client->yp=validity_move.new_y2;


                            event_data2player->character1 = validity_move.character1;
                            event_data2player->x1 = validity_move.new_x1;
                            event_data2player->y1 = validity_move.new_y1;
                            event_data2player->x1_old = x_old;
                            event_data2player->y1_old = y_old;
                            event_data2player->character2 = validity_move.character2;
                            event_data2player->x2 = validity_move.new_x2;
                            event_data2player->y2 = validity_move.new_y2;
                            event_data2player->r1 = rgb_r;
                            event_data2player->g1 = rgb_g;
                            event_data2player->b1 = rgb_b;
                            event_data2player->r2 = rgb_r;
                            event_data2player->g2 = rgb_g;
                            event_data2player->b2 = rgb_b;
                            event_data2player->ID1=ID;
                            event_data2player->ID2=ID;

                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_Change2Characters;
                            new_event.user.data1 = event_data2player;
                            SDL_PushEvent(&new_event);
                            flag_2player = 0;
                        }

                        for (i = 0; i < n_lines ; i++) {
                            printf("%2d ", i);
                            for (j = 0; j < n_cols ; j++) {
                                printf("%3d", board[i][j]);
                            }
                            printf("\n");
                        }
                        sem_post(&inactivity3.sem_inactivity);
                        sem_wait(&arg_sem3.sem_nplay2);
                        sem_post(&arg_sem3.sem_nplay1);
                    }
                }
            }

        }
        pthread_mutex_unlock(&mux);


    }
    //NEED SYNC HERE
    pthread_mutex_lock(&mux);
    board[this_client->yp][this_client->xp]=0;
    board[this_client->ym][this_client->xm]=0;

    event_data_final->xm= this_client->xm;
    event_data_final->ym = this_client->ym;
    event_data_final->xp = this_client->xp;
    event_data_final->yp = this_client->yp;
    //clean this client of client list

    Clients =remove_ID(Clients,this_client);

    //clean this client of the board representation


    //--------------needs to be done---------------//
    //Clean two fuits of the board if Clients!=NULL
    if(Clients!=NULL) {
        event_data_cleanfruit->fruit1=0;
        event_data_cleanfruit->fruit2=0;

        if(Clients->next==NULL){
            Clients->score=0;
            printf("Client ID: %d score: %d \n\n", Clients->ID, Clients->score);
        }

        Cherry_list = get_fruit_list(Fruits, 4);
        if(Cherry_list!=NULL){
            board[Cherry_list->y][Cherry_list->x] = 0;

            event_data_cleanfruit->x1=Cherry_list->x;
            event_data_cleanfruit->y1=Cherry_list->y;
            event_data_cleanfruit->fruit1=4;
            sem_post(&Cherry_list->end_thread_fruit);
            sem_post(&Cherry_list->sem_fruit1);
            pthread_join(Cherry_list->thread_id,NULL);
            Fruits =remove_fruit_list(Fruits, Cherry_list);
        }

        Lemon_list = get_fruit_list(Fruits, 5);
        if(Lemon_list!=NULL){
            board[Lemon_list->y][Lemon_list->x] = 0;

            event_data_cleanfruit->x2=Lemon_list->x;
            event_data_cleanfruit->y2=Lemon_list->y;
            event_data_cleanfruit->fruit2=5;
            sem_post(&Lemon_list->end_thread_fruit);
            sem_post(&Lemon_list->sem_fruit1);
            pthread_join(Lemon_list->thread_id,NULL);
            Fruits =remove_fruit_list(Fruits, Lemon_list);
        }
        //NEED SYNC HERE and free

        SDL_zero(new_event);
        new_event.type = Event_Clean2Fruits;
        new_event.user.data1 = event_data_cleanfruit;
        SDL_PushEvent(&new_event);
        //NEED SYNC HERE and free


    }

    // send the event
    SDL_zero(new_event);
    new_event.type = Event_Disconnect;
    new_event.user.data1 = event_data_final;
    SDL_PushEvent(&new_event);
    pthread_mutex_unlock(&mux);

    sem_post(&inactivity1.sem_end);
    sem_post(&inactivity3.sem_end);

    sem_post(&arg_sem1.sem_nplay1);
    sem_post(&arg_sem1.sem_end);

    sem_post(&arg_sem3.sem_nplay1);
    sem_post(&arg_sem3.sem_end);



    //clear semaphores
    pthread_join(thr_ina1, NULL);
    pthread_join(thr_ina3, NULL);
    pthread_join(thr_nplayspersec1, NULL);
    pthread_join(thr_nplayspersec3, NULL);
    //clear socket
    close(sock_fd);
    free(event_data);
    free(event_data_newfruit);
    free(event_data2player);
    free(event_data_cleanfruit);
    free(event_data_ini);
    free(event_data_final);

    printf("%d %d\n", n_cols, n_lines);
    for ( i = 0 ; i < n_lines; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < n_cols; j++) {
            printf("%d", board[i][j]);
        }
        printf("\n");
    }

    pthread_rwlock_wrlock(&end_mux);
    thread_alive--;
    pthread_rwlock_unlock(&end_mux);

    pthread_detach(pthread_self());
    pthread_exit(NULL);
}

//------------------------------------------------//
// ----------Thread for accepting connection------//
//------------------------------------------------//
int flag_end=0;
int server_socket;
void * thread_Accept(void * argc){
    int ID=1;
    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(client_addr);
    int sock_fd;
    struct threadclient_arg *arg;

    pthread_t clientthread_id;
    arg = malloc(sizeof(struct threadclient_arg));
    while(1){
        printf("waiting for connections\n");
        sock_fd= accept(server_socket,(struct sockaddr *) & client_addr, &size_addr);
        if(sock_fd == -1) {
            perror("accept");
            break;
        }else if (flag_end==1){
            break;
        }
        (*arg).sock_fd=sock_fd;
        (*arg).ID=ID;
        pthread_create(&clientthread_id, NULL, clientThreadi, (void*)arg);
        printf("accepted connection \n");
        ID=ID+2;
    }

    pthread_mutex_lock(&mux);
    IDList *aux = Clients;
    while (aux != NULL) {
        shutdown(aux->socket,SHUT_RD);
        aux = aux->next;
    }
    pthread_mutex_unlock(&mux);

    free(arg);
    pthread_exit(NULL);
}


void * send_scores(void * argc){
    sem_t * arg = (sem_t *) argc;

    IDList *aux;
    int size=0,i,sem_value;
    int * score_msg;
    message m;
    m.action=8;



    while(1){


        for(i=0;i<60;i++){
            sleep(1);
            sem_getvalue(arg, &sem_value);
            if(sem_value==1){
                break;
            }
        }
        if(sem_value==1){
            break;
        }

        pthread_mutex_lock(&mux);
        if(Clients==NULL){
            pthread_mutex_unlock(&mux);
            continue;
        }else{

            if(Clients->next==NULL){
                Clients->score=0;
                printf("Client ID: %d score: %d \n\n", Clients->ID, Clients->score);
            } else {

                aux=Clients;
                size=0;
                while (aux != NULL) {
                    size++;
                    aux = get_next_ID(aux);
                }
                m.size= sizeof(int)*2*size;
                score_msg=(int *)malloc(m.size);
                aux=Clients;
                i=0;
                while (aux != NULL) {
                    score_msg[i*2]=aux->ID;
                    score_msg[i*2+1]=aux->score;
                    i++;
                    aux = get_next_ID(aux);
                }
                aux=Clients;
                while (aux != NULL) {
                    printf("Client ID: %d score: %d \n", aux->ID, aux->score);
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, score_msg, m.size, 0);
                    aux = aux->next;
                }
                free(score_msg);
            }
            pthread_mutex_unlock(&mux);
        }

    }
    pthread_exit(NULL);
}


//------------------------------------------------//
// ----------------MAIN FUNCTION------------------//
//------------------------------------------------//

int main(int argc , char* argv[]){

    int i, j;
    int err;

    SDL_Event event;
    int done=0;

    //------------------------------------------------//
    // ----------create corresponding board --------- //
    //-----------read file from board file-- ---------//
    //------------------------------------------------//
    FILE *fp;
    if ((fp = fopen("board.txt","r"))==NULL){
        printf("File board does not exit\n");
        exit(-1);
    }

    if(fscanf(fp,"%d %d%*c", &n_cols, &n_lines)!=2) {
        printf("Error reading board  file\n");
        exit(-1);
    }

    create_board_window(n_cols, n_lines);
    printf("board size, n_col: %d, n_lines: %d\n", n_cols, n_lines);
    if(n_cols<2 ||n_lines<2){
        printf("Error board is to small\n");
        exit(-1);
    }



    char read_object;
    board =(int **) malloc(sizeof(int *) * (n_lines));
    for ( i = 0 ; i < n_lines; i++) {
        board[i]=(int *)malloc(sizeof(int) * (n_cols));
    }

    for ( i = 0 ; i < n_lines; i++){
        for (j = 0; j < n_cols+1; j++){

            fscanf(fp,"%c", &read_object);
            if(read_object=='B'){
                paint_brick(j, i);
                board[i][j]=1;
            } else if(read_object==' ') {
                board[i][j] = 0;
            } else if(read_object=='\n'){
                continue;
            }else {
                continue;
            }
        }
    }

    for ( i = 0 ; i < n_lines; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < n_cols; j++) {
            printf("%2d", board[i][j]);
        }
        printf("\n");
    }


    fclose(fp);







    //------------------------------------------------//
    // create thread responsible to accept connection //
    //------------------------------------------------//


    pthread_t thread_Accept_id;
    pthread_t thread_score;
    // create thread Accept
    struct sockaddr_in server_local_addr;
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == -1){
        perror("socket: ");
        exit(-1);
    }
    server_local_addr.sin_family = AF_INET;
    server_local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_local_addr.sin_port = htons(50000);
    err = bind(server_socket, (struct sockaddr *)&server_local_addr,sizeof(server_local_addr));
    if(err == -1) {
        perror("bind");
        exit(-1);
    }
    if(listen(server_socket, 20) == -1) {
        perror("listen");
        exit(-1);
    }

    //mutex initial operations
    sem_t ending_sem;

    pthread_mutex_init(&mux,NULL);
    pthread_rwlock_init(&end_mux,NULL);
    sem_init(&ending_sem,0,0);

    //init pthread's
    pthread_create(&thread_Accept_id, NULL, thread_Accept, NULL);
    pthread_create(&thread_score, NULL, send_scores, &ending_sem);


/*------------------------------------------------------------------------//  GAME LOGIC///----------------------------------------------------------*/

    //monster and packman position
    Event_ShowCharacter= SDL_RegisterEvents(1);
    Event_Change2Characters= SDL_RegisterEvents(1);
    Event_ShownewCharacter= SDL_RegisterEvents(1);
    Event_Inactivity= SDL_RegisterEvents(1);
    Event_Disconnect= SDL_RegisterEvents(1);
    Event_ShowFruit=SDL_RegisterEvents(1);
    Event_ShownewFruits=SDL_RegisterEvents(1);

    while (!done){
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = SDL_TRUE;
            }
            //------------------------------------------//
            //--- Show a new PACMAN and new Monster-----//
            //------------------------------------------//

            if (event.type == Event_ShowCharacter) {
                rec_character_pos_msg msg;
                message m;
                m.action=2;
                m.size= sizeof(msg);
                // we get the data (created with the malloc)
                Event_ShowCharacter_Data *data = event.user.data1;
                clear_place(data->x_old, data->y_old);

                msg.character = data->character;
                msg.x1 = data->x;
                msg.y1 = data->y;
                msg.x2 = data->x_old;
                msg.y2 = data->y_old;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;
                msg.ID= data->ID;

                // we paint a pacman
                if (data->character == 1) {
                    paint_pacman(data->x, data->y, data->r, data->g, data->b);
                    printf("move pacman x-%d y-%d\n", data->x,  data->y);
                }
                // we paint a superpacman
                if (data->character == 2) {
                    paint_powerpacman( data->x,  data->y, data->r, data->g, data->b);
                    printf("move super pacman x-%d y-%d\n",  data->x,  data->y);
                }
                //we paint a monster
                if (data->character == 3) {
                    paint_monster( data->x,  data->y, data->r, data->g, data->b);
                    printf("move monster x-%d y-%d\n",  data->x,  data->y);
                }

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
                pthread_mutex_unlock(&mux);
            }

            if (event.type == Event_Inactivity) {
                rec_character_pos_msg msg;
                message m;
                m.action=2;
                m.size= sizeof(msg);
                // we get the data (created with the malloc)
                Event_Inactivity_Data *data = event.user.data1;
                clear_place(data->x_old, data->y_old);

                msg.character = data->character;
                msg.x1 = data->x;
                msg.y1 = data->y;
                msg.x2 = data->x_old;
                msg.y2 = data->y_old;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;
                printf("sdaf:%d\n",data->ID);
                msg.ID=data->ID;


                // we paint a pacman
                if (data->character == 1) {
                    paint_pacman(data->x, data->y, data->r, data->g, data->b);
                    printf("move pacman x-%d y-%d\n", data->x, data->x);
                }
                if (data->character == 2) {
                    paint_pacman(data->x, data->y, data->r, data->g, data->b);
                    printf("move super pacman x-%d y-%d\n", data->x, data->y);
                }
                if (data->character == 3) {
                    paint_monster(data->x, data->y, data->r, data->g, data->b);
                    printf("move monster x-%d y-%d\n", data->x, data->y);
                }

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
                pthread_mutex_unlock(&mux);


            }

            if (event.type == Event_ShowFruit) {
                rec_object_msg msg_fruit;
                message m;
                m.action=3;
                m.size= sizeof(msg_fruit);
                // we get the data (created with the malloc)
                Event_ShowFruit_Data *data = event.user.data1;

                if(data->fruit==4) {
                    msg_fruit.object =4;
                    paint_cherry(data->x,data->y);

                }
                else if(data->fruit==5) {
                    msg_fruit.object =5;
                    paint_lemon(data->x,data->y);
                }
                msg_fruit.x = data->x;
                msg_fruit.y = data->y;
                printf("fruit in x-%d y-%d %d\n", data->x, data->y,data->fruit);


                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg_fruit, m.size, 0);
                    aux = aux->next;
                }
                pthread_mutex_unlock(&mux);


            }
            if (event.type == Event_ShownewFruits) {
                rec_object_msg msg_fruit1;
                rec_object_msg msg_fruit2;

                message m;
                m.action=3;
                m.size= sizeof(msg_fruit1);
                // we get the data (created with the malloc)
                Event_ShownewFruits_Data *data = event.user.data1;
                if(data->fruit1==4){
                    paint_cherry(data->x1,data->y1);

                }
                if(data->fruit2==5){
                    paint_lemon(data->x2,data->y2);
                }
                printf("fruit in -%d y-%d\n", data->x1, data->y1);
                printf("fruit in -%d y-%d\n", data->x2, data->y2);

                msg_fruit1.object=4;
                msg_fruit1.x=data->x1;
                msg_fruit1.y=data->y1;

                msg_fruit2.object=5;
                msg_fruit2.x=data->x2;
                msg_fruit2.y=data->y2;

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    if(data->fruit1==4) {
                        send(aux->socket, &m, sizeof(message), 0);
                        send(aux->socket, &msg_fruit1, m.size, 0);
                    }
                    if(data->fruit2==5) {
                        send(aux->socket, &m, sizeof(message), 0);
                        send(aux->socket, &msg_fruit2, m.size, 0);
                    }
                    aux = aux->next;

                }
                pthread_mutex_unlock(&mux);


            }

            if (event.type == Event_ShownewCharacter) {
                rec_character_pos_msg msg;
                message m;
                m.action=4;
                m.size= sizeof(msg);
                // we get the data (created with the malloc)
                Event_ShownewCharacter_Data *data = event.user.data1;

                paint_pacman(data->xp, data->yp, data->r, data->g, data->b);
                printf("new pacman x-%d y-%d\n", data->xp, data->yp);
                paint_monster(data->xm, data->ym, data->r, data->g, data->b);
                printf("new monster x-%d y-%d\n", data->xm, data->ym);

                msg.character = 0;
                msg.x1 = data->xp;
                msg.y1 = data->yp;
                msg.x2 = data->xm;
                msg.y2 = data->ym;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;
                msg.ID=data->ID;

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
                pthread_mutex_unlock(&mux);


            }

            if (event.type == Event_Change2Characters){
                rec_change2characters_msg msg_c2c;
                message m;
                m.action=5;
                m.size= sizeof(msg_c2c);
                // we get the data (created with the malloc)
                Event_Change2Characters_Data *data = event.user.data1;

                clear_place(data->x1_old, data->y1_old);
                if (data->character1 == 1) {
                    paint_pacman(data->x1, data->y1, data->r1, data->g1, data->b1);
                } else if (data->character1 == 2) {
                    paint_powerpacman(data->x1, data->y1, data->r1, data->g1, data->b1);
                } else if (data->character1 == 3) {
                    paint_monster(data->x1, data->y1, data->r1, data->g1, data->b1);
                }
                if (data->character2 == 1) {
                    paint_pacman(data->x2, data->y2, data->r2, data->g2, data->b2);
                } else if (data->character2 == 2) {
                    paint_powerpacman(data->x2, data->y2, data->r2, data->g2, data->b2);
                } else if (data->character2 == 3) {
                    paint_monster(data->x2, data->y2, data->r2, data->g2, data->b2);
                }

                msg_c2c.character1 = data->character1;
                msg_c2c.x1_old = data->x1_old;
                msg_c2c.y1_old = data->y1_old;
                msg_c2c.x1 = data->x1;
                msg_c2c.y1 = data->y1;
                msg_c2c.r1 = data->r1;
                msg_c2c.g1 = data->g1;
                msg_c2c.b1 = data->b1;
                msg_c2c.ID1=data->ID1;
                msg_c2c.character2 = data->character2;
                msg_c2c.x2 = data->x2;
                msg_c2c.y2 = data->y2;
                msg_c2c.r2 = data->r2;
                msg_c2c.g2 = data->g2;
                msg_c2c.b2 = data->b2;
                msg_c2c.ID2=data->ID2;

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg_c2c, m.size, 0);
                    aux = aux->next;

                }
                pthread_mutex_unlock(&mux);



                //send to all clients
            }

            if (event.type == Event_Disconnect) {
                rec_character_pos_msg msg;
                message m;
                m.action=6;
                m.size= sizeof(msg);
                // we get the data (created with the malloc)
                Event_Disconnect_Data *data = event.user.data1;


                msg.character = 0;
                msg.x1 = data->xm;
                msg.y1 = data->ym;
                msg.x2 = data->xp;
                msg.y2 = data->yp;
                msg.r=0;
                msg.g=0;
                msg.b=0;
                msg.ID=0;

                clear_place(data->xm, data->ym);
                clear_place(data->xp, data->yp);
                printf("clear monster in-%d y-%d\n", data->xm, data->ym);
                printf("clear pacman  in-%d y-%d\n", data->xp, data->yp);

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
                pthread_mutex_unlock(&mux);
        }

            if (event.type == Event_Clean2Fruits) {
                rec_object_msg msg_fruit1;
                rec_object_msg msg_fruit2;

                message m;
                m.action=7;
                m.size= sizeof(msg_fruit1);
                // we get the data (created with the malloc)
                Event_Clean2Fruits_Data *data = event.user.data1;
                clear_place(data->x1,data->y1);
                clear_place(data->x2,data->y2);
                printf("clean fruit in -%d y-%d\n", data->x1, data->y1);
                printf("clean fruit in -%d y-%d\n", data->x2, data->y2);

                msg_fruit1.object=4;
                msg_fruit1.x=data->x1;
                msg_fruit1.y=data->y1;

                msg_fruit2.object=5;
                msg_fruit2.x=data->x2;
                msg_fruit2.y=data->y2;

                //SYNC MECHANISM
                pthread_mutex_lock(&mux);
                IDList *aux = Clients;
                while (aux != NULL) {
                    if(data->fruit1==4) {
                        send(aux->socket, &m, sizeof(message), 0);
                        send(aux->socket, &msg_fruit1, m.size, 0);
                    }
                    if(data->fruit2==5) {
                        send(aux->socket, &m, sizeof(message), 0);
                        send(aux->socket, &msg_fruit2, m.size, 0);
                    }
                    aux = aux->next;
                }
                pthread_mutex_unlock(&mux);

            }
        }
    }


    //---------------close server thread ----------------------------//
    int sock_fd;
    struct sockaddr_in server_addr;
    sock_fd=socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
    if (sock_fd==-1){
        perror("socket: ");
        exit(-1);
    }
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(50000);
    if(inet_aton("127.0.0.1", &server_addr.sin_addr)==0){
        printf("The address is not a valid address \n");
        exit(-1);
    }
    flag_end=1;
    if(connect(sock_fd,(const struct sockaddr *)&server_addr, sizeof(server_addr))==-1){
        perror("Error connecting: ");
        exit(-1);
    }

    pthread_join(thread_Accept_id,NULL);
    close(server_socket);
    close(sock_fd);

    //---------------------------------------------------------------//

    //---------------close score thread -----------------------------//
    sem_post(&ending_sem);
    pthread_join(thread_score,NULL);
    //---------------------------------------------------------------//


    //-------------wait for the other thread to close ---------------//
    while(1){
        pthread_rwlock_rdlock(&end_mux);
        if(thread_alive==0){
            break;
        }
        pthread_rwlock_unlock(&end_mux);
        //wait for all thread to exit
    }
    //---------------------------------------------------------------//

    for ( i = 0 ; i < n_lines; i++) {
        free(board[i]);
    }
    free(board);

    pthread_rwlock_destroy(&end_mux);
    pthread_mutex_destroy(&mux);
    close_board_windows();
    printf("fim\n");

    exit(0);
}
