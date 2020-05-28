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
//Thread responsible for receiving messages from each player//
//----------------and deal with initial protocol------------//
IDList *Clients;
Fruits_Struct *Fruits;
int ** board;
int n_cols;
int n_lines;

Uint32 Event_ShowCharacter;
Uint32 Event_Change2Characters;
Uint32 Event_ShownewCharacter;
Uint32 Event_Inactivity;
Uint32 Event_Disconnect;
Uint32 Event_ShowFruit;
Uint32 Event_ShownewFruits;
Uint32 Event_Clean2Fruits;

//----------------------------------------------------------//
//Thread responsible for receiving messages from each player//
//----------------and deal with initial protocol------------//
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
    while(1){
        sem_wait(&sem_struct_arg->sem_nplay1);
        usleep(500000);
        sem_post(&sem_struct_arg->sem_nplay2);
    }
}

//----------------------------------------------------------//
//---Thread responsible for checking the inactivity of -----//
//---------------a certain character------------------------//
//----------------------------------------------------------//

void * thr_inactivity(void * arg) {
    struct  sem_inactivity * inactivity_arg = (struct sem_inactivity *) arg;
    int sem_value,error;
    int msec = 0, trigger = 300000; /* 30000ms=30s */
    int elem,x,y,x_old,y_old;
    IDList * client_list;
    pos_struct old_pos;
    clock_t difference;


    SDL_Event new_event;
    Event_Inactivity_Data *event_data;
    event_data = malloc(sizeof(Event_Inactivity_Data));
    clock_t before = clock();
    while(1) {
        error =sem_getvalue(&inactivity_arg->sem_inactivity, &sem_value);
        if (sem_value == 0 && error==0) {

            difference = clock() - before;
            msec = difference * 1000 / CLOCKS_PER_SEC;

            if (msec > trigger) {
                //set new random place
                client_list=get_IDlist(Clients, inactivity_arg->ID);
                if(get_IDlist(Clients, inactivity_arg->ID)!=NULL) {
                    if (inactivity_arg->character == 1) {
                        client_list=get_IDlist(Clients, inactivity_arg->ID);
                        x_old = client_list->xp;
                        y_old = client_list->yp;
                        board[y_old][x_old]=0;
                    } else if (inactivity_arg->character == 3) {
                        client_list=get_IDlist(Clients, inactivity_arg->ID);
                        x_old = client_list->xm;
                        y_old = client_list->ym;
                        board[y_old][x_old]=0;
                    }
                }else{
                    pthread_exit(NULL);
                }
                elem = set_initialpos(board, n_cols, n_lines);
                x = elem % n_cols;
                y = elem / n_cols;

                //change x_ID and y_ID
                if(inactivity_arg->character==1){
                    client_list->xp=x;
                    client_list->yp=y;
                }if(inactivity_arg->character==3){
                    client_list->xm=x;
                    client_list->ym=y;
                }

                event_data->character=inactivity_arg->character;
                event_data->x_old = x_old;
                event_data->y_old = y_old;
                event_data->x = x;
                event_data->y= y;
                event_data->r = inactivity_arg->rgb_r;
                event_data->g = inactivity_arg->rgb_g;
                event_data->b = inactivity_arg->rgb_b;

                SDL_zero(new_event);
                new_event.type = Event_Inactivity;
                new_event.user.data1 = event_data;
                SDL_PushEvent(&new_event);

                printf("new position x:%d y:%d \n", x, y);
                printf("old position x:%d y:%d \n", old_pos.x, old_pos.y);
                //reset clock
                before = clock();
            }
            usleep(250000);

        }else if(error==-1){
            pthread_exit(NULL);

        }else if(sem_value==1){
            sem_wait(&inactivity_arg->sem_inactivity);
            //reset clock
            before = clock();
        }else{
            pthread_exit(NULL);
        }
    }

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
    int elem;
    while(1){
        sem_wait(&my_arg->sem_fruit1);
        sem_post(&my_arg->sem_fruit2);
        sleep(2);
        elem=set_initialpos(board, n_cols,n_lines);
        event_data->x =elem%n_cols;
        event_data->y =elem/n_cols;
        event_data->fruit = fruit;

        my_arg->x=elem%n_cols;
        my_arg->y=elem/n_cols;

        //need Sync
        board[elem/n_cols][elem%n_cols]=my_arg->fruit;

        SDL_zero(new_event);
        new_event.type = Event_ShowFruit;
        new_event.user.data1 = event_data;
        SDL_PushEvent(&new_event);
        sem_wait(&my_arg->sem_fruit2);



    }
}
//----------------------------------------------------------//
//---------------CHECK IF A MOVE IS VALID FUNCTION----------//
//----------------------------------------------------------//

typedef struct valid_move{
    int valid; //0 or 1
    int flag2player; //0 1 (second character of other player) 2 (second character of the same player)
    int flag_newfruit; //0 or 1
    int character1,character2,new_x1,new_y1,new_x2,new_y2;
    int other_rgb_r,other_rgb_g,other_rgb_b;
    int other_ID;
}valid_move;

valid_move check_move(IDList *Clients, int x_new, int y_new,int x_old, int y_old, int character, int ID, int superpower) {
    int elem;
    valid_move validity_move;
    validity_move.valid = 0;
    validity_move.flag2player = 0;
    validity_move.flag_newfruit = 0;
    validity_move.character2 = 0;
    validity_move.new_x1 = 0;
    validity_move.new_y1 = 0;
    validity_move.new_x2 = 0;
    validity_move.new_y2 = 0;
    validity_move.other_rgb_r = 0;
    validity_move.other_rgb_b = 0;
    validity_move.other_rgb_b = 0;
    validity_move.other_ID = 0;



    int ID_other;
    IDList *other_list;


    if (((x_new - 1 == x_old || x_new + 1 == x_old) && y_new == y_old) ||((y_new - 1 == y_old || y_new + 1 == y_old) && x_new == x_old)) {

        if (character == 1) {
            //valid
            validity_move.character1=1;

            if (x_new > (n_cols - 1)) {
                if (board[y_new][n_cols - 2] == 0) {
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;

                    board[y_old][x_old]=0;
                    board[y_new][n_cols - 2] = 10 + ID;
                    validity_move.valid = 1;
                } else if (board[y_new][n_cols - 2] == 4){
                    board[y_new][n_cols - 2] = 10 + ID+1;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == 5){
                    board[y_new][n_cols - 2] = 10 + ID+1;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[y_new][n_cols - 2] - 10;
                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = 10 + ID;


                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == -(ID + 10)) {
                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = ID + 10;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }else if (board[y_new][n_cols - 2] <-10) {
                    board[y_old][x_old] = 0;
                    validity_move.character2=3;
                    validity_move.new_x2=n_cols - 2;
                    validity_move.new_y2=y_new;
                    elem=set_initialpos(board, n_cols,n_lines);
                    x_new =elem%n_cols;
                    y_new =elem/n_cols;

                    board[y_new][x_new] = ID + 10;

                    validity_move.flag2player = 0;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }else {
                    validity_move.new_x1 = x_old;
                    validity_move.new_y1 = y_old;

                    board[y_old][x_old]=0;
                    board[validity_move.new_y1][validity_move.new_x1]=(ID+10);
                    validity_move.valid = 1;
                }
            }else if (x_new < 0) {
                if (board[y_new][1] == 0) {
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;

                    board[y_new][1] = 10 + ID;
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;

                } else if (board[y_new][1] == 4){
                    board[y_new][1] = 10 + ID+1;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[y_new][1] == 5){
                    board[y_new][1] = 10 + ID+1;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[y_new][1] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[y_new][1] - 10;
                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = 10 + ID;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][1] == -(ID + 10)) {
                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = ID + 10;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }else if (board[y_new][1] <-10) {
                    board[y_old][x_old] = 0;
                    validity_move.character2=3;
                    validity_move.new_x2=1;
                    validity_move.new_y2=y_new;
                    elem=set_initialpos(board, n_cols,n_lines);
                    x_new =elem%n_cols;
                    y_new =elem/n_cols;

                    board[y_new][x_new] = ID + 10;

                    validity_move.flag2player = 0;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }else {
                    validity_move.new_x1 = x_old;
                    validity_move.new_y1 = y_old;
                    board[y_old][x_old] = 0;

                    board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                    validity_move.valid = 1;

                }
            }
            else if (y_new > (n_lines - 1)){
                if (board[n_lines - 2][x_new] == 0) {
                    board[n_lines - 2][x_new] = 10 + ID;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.new_x1 = x_new;

                    board[y_old][x_old]=0;
                    validity_move.valid = 1;

                }else if (board[n_lines - 2][x_new] == 4){
                    board[n_lines - 2][x_new] = 10 + ID+1;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] == 5){
                    board[n_lines - 2][x_new] = 10 + ID+1;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[n_lines - 2][x_new] - 10;
                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = 10 + ID;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] == -(ID + 10)) {
                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = ID + 10;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;


                }else if (board[n_lines - 2][x_new] <-10) {
                    board[y_old][x_old] = 0;
                    validity_move.character2=3;
                    validity_move.new_x2=x_new;
                    validity_move.new_y2=n_lines - 2;
                    elem=set_initialpos(board, n_cols,n_lines);
                    x_new =elem%n_cols;
                    y_new =elem/n_cols;

                    board[y_new][x_new] = ID + 10;

                    validity_move.flag2player = 0;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                } else {
                    validity_move.new_y1 = y_old;
                    validity_move.new_x1 = x_old;
                    board[y_old][x_old]=0;

                    board[validity_move.new_y1][validity_move.new_x1]=(ID+10);
                    validity_move.valid = 1;

                }
            }else if (y_new < 0) {

                if (board[1][x_new] == 0) {
                    board[1][x_new] = 10 + ID;
                    validity_move.new_y1 = 1;
                    validity_move.new_x1 = x_new;

                    board[y_old][x_old]=0;
                    validity_move.valid = 1;

                }else if (board[1][x_new] == 4){
                    board[1][x_new] = 10 + ID+1;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[1][x_new] == 5){
                    board[1][x_new] = 10 + ID+1;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[1][x_new] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[1][x_new] - 10;
                    board[y_old][x_old] = board[y_new][x_new];
                    board[1][x_new] = 10 + ID;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;
                }else if (board[1][x_new] == -(ID + 10)) {
                    board[y_old][x_old] = board[1][x_new];
                    board[1][x_new] = ID + 10;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;


                }else if (board[1][x_new] <-10) {
                    board[y_old][x_old] = 0;
                    validity_move.character2=3;
                    validity_move.new_x2=x_new;
                    validity_move.new_y2=1;
                    elem=set_initialpos(board, n_cols,n_lines);
                    x_new =elem%n_cols;
                    y_new =elem/n_cols;

                    board[y_new][x_new] = ID + 10;

                    validity_move.flag2player = 0;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }  else {
                    validity_move.new_y1 = y_old;
                    validity_move.new_x1 = x_old;

                    board[y_old][x_old]=0;
                    board[validity_move.new_y1][validity_move.new_x1]=(ID+10);
                    validity_move.valid = 1;
                }
            }else if (board[y_new][x_new] == 4){
                board[y_new][x_new] = 10 + ID+1;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                board[y_old][x_old]=0;

                validity_move.flag_newfruit = 1;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] == 5){
                board[y_new][x_new] = 10 + ID+1;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                board[y_old][x_old]=0;

                validity_move.flag_newfruit = 2;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] > 10) {
                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                ID_other = board[y_new][x_new] - 10;
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = 10 + ID;

                validity_move.flag2player = 1;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                if (ID_other % 2 == 0) {
                    validity_move.character2 = 2;
                    validity_move.other_ID=ID_other-1;

                    other_list = get_IDlist(Clients, ID_other - 1);
                    other_list->xp=x_old;
                    other_list->yp=y_old;

                } else {
                    validity_move.character2 = 1;
                    validity_move.other_ID=ID_other;

                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xp=x_old;
                    other_list->yp=y_old;
                }
                validity_move.other_rgb_r = other_list->rgb_r;
                validity_move.other_rgb_g = other_list->rgb_g;
                validity_move.other_rgb_b = other_list->rgb_b;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] == -(ID + 10)) {
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = ID + 10;

                validity_move.flag2player = 2;
                validity_move.character2 = 3;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;


            }else if (board[y_new][x_new] <-10) {
                board[y_old][x_old] = 0;
                validity_move.character2=3;
                validity_move.new_x2=x_new;
                validity_move.new_y2=y_new;
                elem=set_initialpos(board, n_cols,n_lines);
                x_new =elem%n_cols;
                y_new =elem/n_cols;

                board[y_new][x_new] = ID + 10;

                validity_move.flag2player = 0;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;


            }else if (x_new != x_old) {
                if (x_new > x_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(x_new - 2 >= 0) {
                            if (board[y_new][x_new - 2] == 0) {
                                validity_move.new_x1 = x_new - 2;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == 4){
                                board[y_new][x_new-2] = 10 + ID+1;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == 5){
                                board[y_new][x_new-2] = 10 + ID+1;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new][x_new-2] - 10;
                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = 10 + ID;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = ID + 10;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }else if (board[y_new][x_new-2] <-10) {
                                board[y_old][x_old] = 0;
                                validity_move.character2=3;
                                validity_move.new_x2=x_new-2;
                                validity_move.new_y2=y_new;
                                elem=set_initialpos(board, n_cols,n_lines);
                                x_new =elem%n_cols;
                                y_new =elem/n_cols;

                                board[y_new][x_new] = ID + 10;

                                validity_move.flag2player = 0;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            } else {
                                validity_move.new_x1 = x_old;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                validity_move.valid = 1;
                            }
                        } else {
                            validity_move.new_x1 = x_old;
                            validity_move.new_y1 = y_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                } else if (x_new < x_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(x_new + 2 <= n_cols - 1) {
                            if (board[y_new][x_new + 2] == 0) {
                                validity_move.new_x1 = x_new + 2;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;

                            }else if (board[y_new][x_new+2] == 4){
                                board[y_new][x_new+2] = 10 + ID+1;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] == 5){
                                board[y_new][x_new+2] = 10 + ID+1;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new][x_new+2] - 10;
                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = 10 + ID;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = ID + 10;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }else if (board[y_new][x_new+2] <-10) {
                                board[y_old][x_old] = 0;
                                validity_move.character2=3;
                                validity_move.new_x2=x_new+2;
                                validity_move.new_y2=y_new;
                                elem=set_initialpos(board, n_cols,n_lines);
                                x_new =elem%n_cols;
                                y_new =elem/n_cols;

                                board[y_new][x_new] = ID + 10;

                                validity_move.flag2player = 0;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }  else {
                                validity_move.new_x1 = x_old;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_x1 = x_old;
                            validity_move.new_y1 = y_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                }else{
                    validity_move.new_y1 = y_new;
                    validity_move.new_x1 = x_new;
                    board[y_old][x_old]=0;
                    board[y_new][x_new]=(ID+10);
                    validity_move.valid=1;
                }
            }else if (y_new != y_old) {
                if (y_new > y_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(y_new - 2 >= 0) {
                            if (board[y_new - 2][x_new] == 0 ) {
                                validity_move.new_y1 = y_new - 2;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == 4){
                                board[y_new-2][x_new] = 10 + ID+1;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == 5){
                                board[y_new-2][x_new] = 10 + ID+1;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new-2][x_new] - 10;
                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = 10 + ID;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = ID + 10;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;


                            }else if (board[y_new-2][x_new] <-10) {
                                board[y_old][x_old] = 0;
                                validity_move.character2=3;
                                validity_move.new_x2=x_new;
                                validity_move.new_y2=y_new-2;
                                elem=set_initialpos(board, n_cols,n_lines);
                                x_new =elem%n_cols;
                                y_new =elem/n_cols;

                                board[y_new][x_new] = ID + 10;

                                validity_move.flag2player = 0;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }  else {
                                validity_move.new_y1 = y_old;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                validity_move.valid = 1;
                            }
                        } else {
                            validity_move.new_y1 = y_old;
                            validity_move.new_x1 = x_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                            validity_move.valid = 1;
                        }
                    }

                }
                else if (y_new < y_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(y_new + 2 <= n_lines - 1) {
                            if (board[y_new + 2][x_new] == 0 ) {
                                validity_move.new_y1 = y_new + 2;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == 4){
                                board[y_new+2][x_new] = 10 + ID+1;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == 5){
                                board[y_new+2][x_new] = 10 + ID+1;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new+2][x_new] - 10;
                                board[y_old][x_old] = board[y_new][x_new];
                                board[y_new+2][x_new] = 10 + ID;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new+2][x_new];
                                board[y_new+2][x_new] = ID + 10;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;


                            }else if (board[y_new+2][x_new] <-10) {
                                board[y_old][x_old] = 0;
                                validity_move.character2=3;
                                validity_move.new_x2=x_new;
                                validity_move.new_y2=y_new+2;
                                elem=set_initialpos(board, n_cols,n_lines);
                                x_new =elem%n_cols;
                                y_new =elem/n_cols;

                                board[y_new][x_new] = ID + 10;

                                validity_move.flag2player = 0;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }  else {
                                validity_move.new_y1 = y_old;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_y1 = y_old;
                            validity_move.new_x1 = x_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                }else{
                    validity_move.new_y1 = y_new;
                    validity_move.new_x1 = x_new;
                    board[y_new][x_new]=(ID+10);
                    board[y_old][x_old]=0;
                    validity_move.valid=1;
                }
            }
            return validity_move;
        }
            //-------------------------------------------------------------------------------//

            //-------------------------------------------------------------------------------//

            //-------------------------------------------------------------------------------//

        else if (character == 2){
            //valid
            validity_move.character1=2;

            if (x_new > (n_cols - 1)) {
                if (board[y_new][n_cols - 2] == 0) {
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;

                    board[y_new][n_cols - 2] = 10 + ID+1;
                    board[y_old][x_old]=0;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == 4){
                    if(superpower==2){
                        board[y_new][n_cols - 2] = 10 + ID;
                    }else{
                        board[y_new][n_cols - 2] = 10 + ID+1;
                    }
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == 5){
                    if(superpower==2){
                        board[y_new][n_cols - 2] = 10 + ID;
                    }else{
                        board[y_new][n_cols - 2] = 10 + ID+1;
                    }
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[y_new][n_cols - 2] - 10;
                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = 10 + ID+1;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == -(ID + 10)) {
                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = ID + 10+1;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }else if (board[y_new][n_cols - 2] < -10) {

                    //monster and eats it
                    ID_other = (-board[y_new][n_cols - 2])-10 ;
                    board[y_old][x_old] = 0;
                    board[y_new][n_cols - 2] = 10 + ID+1;
                    validity_move.flag2player = 3;
                    validity_move.character2 = 3;

                    elem = set_initialpos(board, n_cols, n_lines);

                    board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                    validity_move.new_x2 = elem % n_cols;
                    validity_move.new_y2 = elem / n_cols;
                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm = elem % n_cols;
                    other_list->ym = elem / n_cols;

                    validity_move.other_ID=ID_other;
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                } else {
                    validity_move.new_x1 = x_old;
                    validity_move.new_y1 = y_old;
                    board[y_old][x_old]=0;

                    board[validity_move.new_y1][validity_move.new_x1]=(ID+10)+1;
                    validity_move.valid = 1;
                }
            }else if (x_new < 0) {
                if (board[y_new][1] == 0) {
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;

                    board[y_new][1] = 10 + ID+1;
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;

                } else if (board[y_new][1] == 4){
                    if(superpower==2){
                        board[y_new][1] = 10 + ID;
                    }else{
                        board[y_new][1] = 10 + ID+1;
                    }
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[y_new][1] == 5){
                    if(superpower==2){
                        board[y_new][1] = 10 + ID;
                    }else{
                        board[y_new][1] = 10 + ID+1;
                    }
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[y_new][1] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[y_new][1] - 10;
                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = 10 + ID+1;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][1] == -(ID + 10)) {
                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = ID + 10+1;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                }else if (board[y_new][1] < -10) {

                    //monster and eats it
                    ID_other = (-board[y_new][1])-10 ;
                    board[y_old][x_old] = 0;
                    board[y_new][1] = 10 + ID+1;
                    validity_move.flag2player = 3;
                    validity_move.character2 = 3;

                    elem = set_initialpos(board, n_cols, n_lines);

                    board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                    validity_move.new_x2 = elem % n_cols;
                    validity_move.new_y2 = elem / n_cols;
                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm = elem % n_cols;
                    other_list->ym = elem / n_cols;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.other_ID=ID_other;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;


                } else {
                    validity_move.new_x1 = x_old;
                    validity_move.new_y1 = y_old;
                    board[y_old][x_old] = 0;

                    board[validity_move.new_y1][validity_move.new_x1] = (ID + 10)+1;
                    validity_move.valid = 1;

                }
            }
            else if (y_new > (n_lines - 1)){
                if (board[n_lines - 2][x_new] == 0) {
                    board[n_lines - 2][x_new] = 10 + ID+1;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.new_x1 = x_new;

                    board[y_old][x_old]=0;
                    validity_move.valid = 1;

                } else if (board[n_lines - 2][x_new] == 4){
                    if(superpower==2){
                        board[n_lines - 2][x_new] = 10 + ID;
                    }else{
                        board[n_lines - 2][x_new] = 10 + ID+1;
                    }
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] == 5){
                    if(superpower==2){
                        board[n_lines - 2][x_new] = 10 + ID;
                    }else{
                        board[n_lines - 2][x_new] = 10 + ID+1;
                    }
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[n_lines - 2][x_new] - 10;
                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = 10 + ID+1;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] == -(ID + 10)) {
                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = ID + 10+1;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;


                }else if (board[n_lines - 2][x_new] < -10) {

                    //monster and eats it
                    ID_other = (-board[n_lines - 2][x_new])-10 ;
                    board[y_old][x_old] = 0;
                    board[n_lines - 2][x_new] = 10 + ID+1;
                    validity_move.flag2player = 3;
                    validity_move.character2 = 3;

                    elem = set_initialpos(board, n_cols, n_lines);

                    board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                    validity_move.new_x2 = elem % n_cols;
                    validity_move.new_y2 = elem / n_cols;
                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm = elem % n_cols;
                    other_list->ym = elem / n_cols;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.other_ID=ID_other;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;


                }else {
                    validity_move.new_y1 = y_old;
                    validity_move.new_x1 = x_old;
                    board[y_old][x_old]=0;

                    board[validity_move.new_y1][validity_move.new_x1]=(ID+10)+1;
                    validity_move.valid = 1;

                }
            }else if (y_new < 0) {

                if (board[1][x_new] == 0) {
                    board[1][x_new] = 10 + ID+1;
                    validity_move.new_y1 = 1;
                    validity_move.new_x1 = x_new;

                    board[y_old][x_old]=0;
                    validity_move.valid = 1;

                }else if (board[1][x_new] == 4){
                    if(superpower==2){
                        board[1][x_new] = 10 + ID;
                    }else{
                        board[1][x_new] = 10 + ID+1;
                    }
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;
                }else if (board[1][x_new] == 5){
                    if(superpower==2){
                        board[1][x_new] = 10 + ID;
                    }else{
                        board[1][x_new] = 10 + ID+1;
                    }
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                }else if (board[1][x_new] > 10) {
                    //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                    ID_other = board[1][x_new] - 10;
                    board[y_old][x_old] = board[1][x_new];
                    board[1][x_new] = 10 + ID+1;

                    validity_move.flag2player = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    if (ID_other % 2 == 0) {
                        validity_move.character2 = 2;
                        validity_move.other_ID=ID_other-1;

                        other_list = get_IDlist(Clients, ID_other - 1);
                        other_list->xp=x_old;
                        other_list->yp=y_old;

                    } else {
                        validity_move.character2 = 1;
                        validity_move.other_ID=ID_other;

                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp=x_old;
                        other_list->yp=y_old;
                    }
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;
                }else if (board[1][x_new] == -(ID + 10)) {
                    board[y_old][x_old] = board[1][x_new];
                    board[1][x_new] = ID + 10+1;

                    validity_move.flag2player = 2;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;


                }else if (board[1][x_new] < -10) {

                    //monster and eats it
                    ID_other = (-board[1][x_new])-10 ;
                    board[y_old][x_old] = 0;
                    board[1][x_new] = 10 + ID+1;
                    validity_move.flag2player = 3;
                    validity_move.character2 = 3;

                    elem = set_initialpos(board, n_cols, n_lines);

                    board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                    validity_move.new_x2 = elem % n_cols;
                    validity_move.new_y2 = elem / n_cols;
                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm = elem % n_cols;
                    other_list->ym = elem / n_cols;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.other_ID=ID_other;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;


                } else {
                    validity_move.new_y1 = y_old;
                    validity_move.new_x1 = x_old;
                    board[y_old][x_old]=0;

                    board[validity_move.new_y1][validity_move.new_x1]=(ID+10)+1;
                    validity_move.valid = 1;
                }


            }else if (board[y_new][x_new] == 4){
                if(superpower==2){
                    board[y_new][x_new] = 10 + ID;
                }else{
                    board[y_new][x_new] = 10 + ID+1;
                }
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                board[y_old][x_old]=0;

                validity_move.flag_newfruit = 1;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] == 5){
                if(superpower==2){
                    board[y_new][x_new] = 10 + ID;
                }else{
                    board[y_new][x_new] = 10 + ID+1;
                }
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                board[y_old][x_old]=0;

                validity_move.flag_newfruit = 2;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] > 10) {
                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                ID_other = board[y_new][x_new] - 10;
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = 10 + ID+1;

                validity_move.flag2player = 1;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                if (ID_other % 2 == 0) {
                    validity_move.character2 = 2;
                    validity_move.other_ID=ID_other-1;

                    other_list = get_IDlist(Clients, ID_other - 1);
                    other_list->xp=x_old;
                    other_list->yp=y_old;

                } else {
                    validity_move.character2 = 1;
                    validity_move.other_ID=ID_other;

                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xp=x_old;
                    other_list->yp=y_old;
                }
                validity_move.other_rgb_r = other_list->rgb_r;
                validity_move.other_rgb_g = other_list->rgb_g;
                validity_move.other_rgb_b = other_list->rgb_b;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] == -(ID + 10)) {
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = ID + 10+1;

                validity_move.flag2player = 2;
                validity_move.character2 = 3;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;


            }else if (board[y_new][x_new] < -10) {

                //monster and eats it
                ID_other = (-board[y_new][x_new])-10 ;
                board[y_old][x_old] = 0;
                board[y_new][x_new] = 10 + ID+1;
                validity_move.flag2player = 3;
                validity_move.character2 = 3;

                elem = set_initialpos(board, n_cols, n_lines);

                board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                validity_move.new_x2 = elem % n_cols;
                validity_move.new_y2 = elem / n_cols;
                other_list = get_IDlist(Clients, ID_other);
                other_list->xm = elem % n_cols;
                other_list->ym = elem / n_cols;

                validity_move.other_rgb_r = other_list->rgb_r;
                validity_move.other_rgb_g = other_list->rgb_g;
                validity_move.other_rgb_b = other_list->rgb_b;
                validity_move.other_ID=ID_other;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;


            }else if (x_new != x_old) {
                if (x_new > x_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(x_new - 2 >= 0) {
                            if (board[y_new][x_new - 2] == 0 ) {
                                validity_move.new_x1 = x_new - 2;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == 4){
                                if(superpower==2){
                                    board[y_new][x_new-2] = 10 + ID;
                                }else{
                                    board[y_new][x_new-2] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == 5){
                                if(superpower==2){
                                    board[y_new][x_new-2] = 10 + ID;
                                }else{
                                    board[y_new][x_new-2] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new][x_new-2] - 10;
                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = 10 + ID+1;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = ID + 10+1;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }else if (board[y_new][x_new-2] < -10) {

                                //monster and eats it
                                ID_other = (-board[y_new][x_new-2])-10 ;
                                board[y_old][x_old] = 0;
                                board[y_new][x_new-2] = 10 + ID+1;
                                validity_move.flag2player = 3;
                                validity_move.character2 = 3;

                                elem = set_initialpos(board, n_cols, n_lines);

                                board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                                validity_move.new_x2 = elem % n_cols;
                                validity_move.new_y2 = elem / n_cols;
                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm = elem % n_cols;
                                other_list->ym = elem / n_cols;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.other_ID=ID_other;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            } else {
                                validity_move.new_x1 = x_old;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_x1 = x_old;
                            validity_move.new_y1 = y_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                            validity_move.valid = 1;
                        }
                    }
                } else if (x_new < x_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(x_new + 2 <= n_cols - 1) {
                            if (board[y_new][x_new + 2] == 0) {
                                validity_move.new_x1 = x_new + 2;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;

                            } else if (board[y_new][x_new+2] == 4){
                                if(superpower==2){
                                    board[y_new][x_new+2] = 10 + ID;
                                }else{
                                    board[y_new][x_new+2] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] == 5){
                                if(superpower==2){
                                    board[y_new][x_new+2] = 10 + ID;
                                }else{
                                    board[y_new][x_new+2] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new][x_new+2] - 10;
                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = 10 + ID+1;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = ID + 10+1;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }else if (board[y_new][x_new+2] < -10) {

                                //monster and eats it
                                ID_other = (-board[y_new][x_new+2])-10 ;
                                board[y_old][x_old] = 0;
                                board[y_new][x_new+2] = 10 + ID+1;
                                validity_move.flag2player = 3;
                                validity_move.character2 = 3;

                                elem = set_initialpos(board, n_cols, n_lines);

                                board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                                validity_move.new_x2 = elem % n_cols;
                                validity_move.new_y2 = elem / n_cols;
                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm = elem % n_cols;
                                other_list->ym = elem / n_cols;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.other_ID=ID_other;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;


                            }else {
                                validity_move.new_x1 = x_old;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                validity_move.valid = 1;
                            }
                        } else {
                            validity_move.new_x1 = x_old;
                            validity_move.new_y1 = y_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                            validity_move.valid = 1;
                        }
                    }
                }else{
                    validity_move.new_y1 = y_new;
                    validity_move.new_x1 = x_new;
                    board[y_old][x_old]=0;
                    board[y_new][x_new]=(ID+10)+1;
                    validity_move.valid=1;
                }
            }else if (y_new != y_old) {
                if (y_new > y_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(y_new - 2 >= 0) {
                            if (board[y_new - 2][x_new] == 0 ) {
                                validity_move.new_y1 = y_new - 2;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == 4){
                                if(superpower==2){
                                    board[y_new-2][x_new] = 10 + ID;
                                }else{
                                    board[y_new-2][x_new] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == 5){
                                if(superpower==2){
                                    board[y_new-2][x_new] = 10 + ID;
                                }else{
                                    board[y_new-2][x_new] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new-2][x_new] - 10;
                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = 10 + ID+1;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = ID + 10+1;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;


                            }else if (board[y_new-2][x_new] < -10) {

                                //monster and eats it
                                ID_other = (-board[y_new-2][x_new])-10 ;
                                board[y_old][x_old] = 0;
                                board[y_new-2][x_new] = 10 + ID+1;
                                validity_move.flag2player = 3;
                                validity_move.character2 = 3;

                                elem = set_initialpos(board, n_cols, n_lines);

                                board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                                validity_move.new_x2 = elem % n_cols;
                                validity_move.new_y2 = elem / n_cols;
                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm = elem % n_cols;
                                other_list->ym = elem / n_cols;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.other_ID=ID_other;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;


                            } else {
                                validity_move.new_y1 = y_old;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_y1 = y_old;
                            validity_move.new_x1 = x_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                            validity_move.valid = 1;
                        }
                    }
                }
                else if (y_new < y_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(y_new + 2 <= n_lines - 1) {
                            if (board[y_new + 2][x_new] != 1) {
                                validity_move.new_y1 = y_new + 2;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == 4){
                                if(superpower==2){
                                    board[y_new+2][x_new] = 10 + ID;
                                }else{
                                    board[y_new+2][x_new] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == 5){
                                if(superpower==2){
                                    board[y_new+2][x_new] = 10 + ID;
                                }else{
                                    board[y_new+2][x_new] = 10 + ID+1;
                                }
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] > 10) {
                                //printf("board[y_new][x_new]:%d\n",board[y_new][x_new]);
                                ID_other = board[y_new+2][x_new] - 10;
                                board[y_old][x_old] = board[y_new+2][x_new];
                                board[y_new+2][x_new] = 10 + ID+1;

                                validity_move.flag2player = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                if (ID_other % 2 == 0) {
                                    validity_move.character2 = 2;
                                    validity_move.other_ID=ID_other-1;

                                    other_list = get_IDlist(Clients, ID_other - 1);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;

                                } else {
                                    validity_move.character2 = 1;
                                    validity_move.other_ID=ID_other;

                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp=x_old;
                                    other_list->yp=y_old;
                                }
                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == -(ID + 10)) {
                                board[y_old][x_old] = board[y_new+2][x_new];
                                board[y_new+2][x_new] = ID + 10+1;

                                validity_move.flag2player = 2;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;


                            }else if (board[y_new+2][x_new] < -10) {

                                //monster and eats it
                                ID_other = (-board[y_new+2][x_new])-10 ;
                                board[y_old][x_old] = 0;
                                board[y_new+2][x_new] = 10 + ID+1;
                                validity_move.flag2player = 3;
                                validity_move.character2 = 3;

                                elem = set_initialpos(board, n_cols, n_lines);

                                board[elem / n_cols][elem % n_cols] = -(10 + ID_other);
                                validity_move.new_x2 = elem % n_cols;
                                validity_move.new_y2 = elem / n_cols;
                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm = elem % n_cols;
                                other_list->ym = elem / n_cols;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.other_ID=ID_other;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;


                            }  else {
                                validity_move.new_y1 = y_old;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_y1 = y_old;
                            validity_move.new_x1 = x_new;

                            board[validity_move.new_y1][validity_move.new_x1] = (ID + 10) + 1;
                            validity_move.valid = 1;
                        }
                    }
                }else{
                    validity_move.new_y1 = y_new;
                    validity_move.new_x1 = x_new;
                    board[y_new][x_new]=(ID+10)+1;
                    board[y_old][x_old]=0;
                    validity_move.valid=1;
                }
            }
            return validity_move;
        }
            //-------------------------------------------------------------------------------//

            //-------------------------------------------------------------------------------//

            //-------------------------------------------------------------------------------//

        else if (character == 3) {
            validity_move.character1 = 3;

            if (x_new > (n_cols - 1)) {
                if (board[y_new][n_cols - 2] == 0) {
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;

                    board[y_new][n_cols - 2] = -(10 + ID);
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == 4){
                    board[y_new][n_cols - 2] = -(10 + ID);
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;

                }else if (board[y_new][n_cols - 2] == 5){
                    board[y_new][n_cols - 2] = -(10 + ID);
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                } else if (board[y_new][n_cols - 2] < -10) {
                    ID_other = (-10) - (board[y_new][n_cols - 2]);
                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = -(10 + ID);
                    validity_move.flag2player = 1;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;

                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm=x_old;
                    other_list->ym=y_old;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.other_ID=ID_other;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                    //monster of another player
                } else if (board[y_new][n_cols - 2] == (ID + 10)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] == (ID + 10+1)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[y_new][n_cols - 2];
                    board[y_new][n_cols - 2] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 2;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = n_cols - 2;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][n_cols - 2] >10) {
                    if(board[y_new][n_cols - 2]%2==0){
                        ID_other = board[y_new][n_cols - 2]-10-1;
                        other_list = get_IDlist(Clients, ID_other);
                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.other_ID=ID_other;
                        validity_move.new_x2=other_list->xp;
                        validity_move.new_y2=other_list->yp;

                        //superpowerpacman
                        //gets eaten
                        board[y_old][x_old] = 0;
                        elem=set_initialpos(board, n_cols,n_lines);
                        x_new =elem%n_cols;
                        y_new =elem/n_cols;


                        board[y_new][x_new] = -(ID + 10);
                        validity_move.flag2player = 4;
                        validity_move.new_x1 = x_new;
                        validity_move.new_y1 = y_new;
                        validity_move.valid = 1;

                    }else {
                        //pacman
                        // eats it
                        ID_other = board[y_new][n_cols - 2] - 10;
                        board[y_old][x_old] = 0;
                        board[y_new][n_cols - 2] = -(10 + ID);
                        validity_move.flag2player = 1;
                        validity_move.character2 = 1;

                        elem = set_initialpos(board, n_cols, n_lines);

                        board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                        validity_move.new_x2 = elem % n_cols;
                        validity_move.new_y2 = elem / n_cols;
                        validity_move.other_ID=ID_other;
                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp = elem % n_cols;
                        other_list->yp = elem / n_cols;

                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x1 = n_cols - 2;
                        validity_move.new_y1 = y_new;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move.new_x1 = x_old;
                    validity_move.new_y1 = y_old;
                    board[y_old][x_old] = 0;

                    board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                    validity_move.valid = 1;
                }
            } else if (x_new < 0) {
                if (board[y_new][1] == 0) {
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;

                    board[y_new][1] = -(10 + ID);
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;

                }else if (board[y_new][1] == 4){
                    board[y_new][1] = -(10 + ID);
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;

                }else if (board[y_new][1] == 5){
                    board[y_new][1] = -(10 + ID);
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                } else if (board[y_new][1] < -10) {
                    ID_other = (-10) - (board[y_new][1]);
                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = -(10 + ID);
                    validity_move.flag2player = 1;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.other_ID=ID_other;

                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm=x_old;
                    other_list->ym=y_old;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                    //monster of another player
                } else if (board[y_new][1] == (ID + 10)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                } else if (board[y_new][1] == (ID + 10+1)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[y_new][1];
                    board[y_new][1] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 2;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = 1;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }else if (board[y_new][1] >10) {
                    if(board[y_new][1]%2==0){
                        ID_other = board[y_new][1]-10-1;
                        other_list = get_IDlist(Clients, ID_other);
                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x2=other_list->xp;
                        validity_move.new_y2=other_list->yp;
                        validity_move.other_ID=ID_other;

                        //superpowerpacman
                        //gets eaten
                        board[y_old][x_old] = 0;
                        elem=set_initialpos(board, n_cols,n_lines);
                        x_new =elem%n_cols;
                        y_new =elem/n_cols;


                        board[y_new][x_new] = -(ID + 10);
                        validity_move.flag2player = 4;
                        validity_move.new_x1 = x_new;
                        validity_move.new_y1 = y_new;
                        validity_move.valid = 1;

                    }else {
                        //pacman
                        // eats it
                        ID_other = board[y_new][1] - 10;
                        board[y_old][x_old] = 0;
                        board[y_new][1] = -(10 + ID);
                        validity_move.flag2player = 1;
                        validity_move.character2 = 1;

                        elem = set_initialpos(board, n_cols, n_lines);

                        board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                        validity_move.new_x2 = elem % n_cols;
                        validity_move.new_y2 = elem / n_cols;
                        validity_move.other_ID=ID_other;
                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp = elem % n_cols;
                        other_list->yp = elem / n_cols;

                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x1 = 1;
                        validity_move.new_y1 = y_new;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move.new_x1 = x_old;
                    validity_move.new_y1 = y_old;
                    board[y_old][x_old] = 0;

                    board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                    validity_move.valid = 1;

                }
            } else if (y_new > (n_lines - 1)) {
                if (board[n_lines - 2][x_new] == 0) {
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.new_x1 = x_new;

                    board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;

                }else if (board[n_lines - 2][x_new] == 4){
                    board[n_lines - 2][x_new] = -(10 + ID);
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;

                }else if (board[n_lines - 2][x_new] == 5){
                    board[n_lines - 2][x_new] = -(10 + ID);
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                } else if (board[n_lines-2][x_new] < -10) {
                    ID_other = (-10) - (board[n_lines - 2][x_new]);
                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = -(10 + ID);
                    validity_move.flag2player = 1;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.other_ID=ID_other;

                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm=x_old;
                    other_list->ym=y_old;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;
                    //monster of another player
                } else if (board[n_lines - 2][x_new] == (ID + 10)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] == (ID + 10+1)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[n_lines - 2][x_new];
                    board[n_lines - 2][x_new] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 2;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = n_lines - 2;
                    validity_move.valid = 1;
                }else if (board[n_lines - 2][x_new] >10) {
                    if(board[n_lines - 2][x_new]%2==0){
                        ID_other = board[n_lines - 2][x_new]-10-1;
                        other_list = get_IDlist(Clients, ID_other);
                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x2=other_list->xp;
                        validity_move.new_y2=other_list->yp;
                        validity_move.other_ID=ID_other;

                        //superpowerpacman
                        //gets eaten
                        board[y_old][x_old] = 0;
                        elem=set_initialpos(board, n_cols,n_lines);
                        x_new =elem%n_cols;
                        y_new =elem/n_cols;


                        board[y_new][x_new] = -(ID + 10);
                        validity_move.flag2player = 4;
                        validity_move.new_x1 = x_new;
                        validity_move.new_y1 = y_new;
                        validity_move.valid = 1;

                    }else {
                        //pacman
                        // eats it
                        printf("32\n");
                        ID_other = board[n_lines - 2][x_new] - 10;
                        board[y_old][x_old] = 0;
                        board[n_lines - 2][x_new] = -(10 + ID);
                        validity_move.flag2player = 1;
                        validity_move.character2 = 1;

                        elem = set_initialpos(board, n_cols, n_lines);

                        board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                        validity_move.new_x2 = elem % n_cols;
                        validity_move.new_y2 = elem / n_cols;
                        validity_move.other_ID=ID_other;
                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp = elem % n_cols;
                        other_list->yp = elem / n_cols;

                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x1 = x_new;
                        validity_move.new_y1 = n_lines - 2;
                        validity_move.valid = 1;
                    }
                } else {
                    validity_move.new_y1 = y_old;
                    validity_move.new_x1 = x_old;
                    board[y_old][x_old] = 0;

                    board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                    validity_move.valid = 1;

                }
            } else if (y_new < 0) {
                if (board[1][x_new] == 0) {
                    validity_move.new_y1 = 1;
                    validity_move.new_x1 = x_new;

                    board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;

                } else if (board[1][x_new] == 4){
                    board[1][x_new] = -(10 + ID);
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 1;
                    validity_move.valid = 1;

                }else if (board[1][x_new] == 5){
                    board[1][x_new] = -(10 + ID);
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    board[y_old][x_old]=0;

                    validity_move.flag_newfruit = 2;
                    validity_move.valid = 1;
                } else if (board[1][x_new] < -10) {
                    ID_other = (-10) - (board[1][x_new]);
                    board[y_old][x_old] = board[1][x_new];
                    board[1][x_new] = -(10 + ID);
                    validity_move.flag2player = 1;
                    validity_move.character2 = 3;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.other_ID=ID_other;

                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xm=x_old;
                    other_list->ym=y_old;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;
                    //monster of another player
                } else if (board[1][x_new] == (ID + 10)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[1][x_new];
                    board[1][x_new] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 1;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;
                }else if (board[1][x_new] == (ID + 10+1)) {
                    //the pacman of the same player

                    board[y_old][x_old] = board[1][x_new];
                    board[1][x_new] = -(ID + 10);
                    validity_move.flag2player = 2;
                    validity_move.character2 = 2;
                    validity_move.new_x2 = x_old;
                    validity_move.new_y2 = y_old;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = 1;
                    validity_move.valid = 1;
                }else if (board[1][x_new] >10) {
                    if(board[1][x_new]%2==0){
                        ID_other = board[1][x_new]-10-1;
                        other_list = get_IDlist(Clients, ID_other);
                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x2=other_list->xp;
                        validity_move.new_y2=other_list->yp;
                        validity_move.other_ID=ID_other;

                        //superpowerpacman
                        //gets eaten
                        board[y_old][x_old] = 0;
                        elem=set_initialpos(board, n_cols,n_lines);
                        x_new =elem%n_cols;
                        y_new =elem/n_cols;


                        board[y_new][x_new] = -(ID + 10);
                        validity_move.flag2player = 4;
                        validity_move.new_x1 = x_new;
                        validity_move.new_y1 = y_new;
                        validity_move.valid = 1;

                    }else {
                        //pacman
                        // eats it
                        ID_other = board[1][x_new] - 10;
                        board[y_old][x_old] = 0;
                        board[1][x_new] = -(10 + ID);
                        validity_move.flag2player = 1;
                        validity_move.character2 = 1;

                        elem = set_initialpos(board, n_cols, n_lines);

                        board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                        validity_move.new_x2 = elem % n_cols;
                        validity_move.new_y2 = elem / n_cols;
                        validity_move.other_ID=ID_other;
                        other_list = get_IDlist(Clients, ID_other);
                        other_list->xp = elem % n_cols;
                        other_list->yp = elem / n_cols;

                        validity_move.other_rgb_r = other_list->rgb_r;
                        validity_move.other_rgb_g = other_list->rgb_g;
                        validity_move.other_rgb_b = other_list->rgb_b;
                        validity_move.new_x1 = x_new;
                        validity_move.new_y1 = 1;
                        validity_move.valid = 1;
                    }
                }else {
                    validity_move.new_y1 = y_old;
                    validity_move.new_x1 = x_old;
                    board[y_old][x_old] = 0;

                    board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                    validity_move.valid = 1;
                }
            }else if (board[y_new][x_new] == 4){
                board[y_new][x_new] = -(10 + ID);
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                board[y_old][x_old]=0;

                validity_move.flag_newfruit = 1;
                validity_move.valid = 1;

            }else if (board[y_new][x_new] == 5){
                board[y_new][x_new] = -(10 + ID);
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                board[y_old][x_old]=0;

                validity_move.flag_newfruit = 2;
                validity_move.valid = 1;
            } else if (board[y_new][x_new] < -10) {
                ID_other = (-10) - (board[y_new][x_new]);
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = -(10 + ID);
                validity_move.flag2player = 1;
                validity_move.character2 = 3;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                validity_move.other_ID=ID_other;

                other_list = get_IDlist(Clients, ID_other);
                other_list->xm=x_old;
                other_list->ym=y_old;

                validity_move.other_rgb_r = other_list->rgb_r;
                validity_move.other_rgb_g = other_list->rgb_g;
                validity_move.other_rgb_b = other_list->rgb_b;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;
                //monster of another player
            } else if (board[y_new][x_new] == (ID + 10)) {
                //the pacman of the same player
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = -(ID + 10);
                validity_move.flag2player = 2;
                validity_move.character2 = 1;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] == (ID + 10+1)) {
                //the pacman of the same player
                board[y_old][x_old] = board[y_new][x_new];
                board[y_new][x_new] = -(ID + 10);
                validity_move.flag2player = 2;
                validity_move.character2 = 2;
                validity_move.new_x2 = x_old;
                validity_move.new_y2 = y_old;
                validity_move.new_x1 = x_new;
                validity_move.new_y1 = y_new;
                validity_move.valid = 1;
            }else if (board[y_new][x_new] >10) {
                if(board[y_new][x_new]%2==0){
                    ID_other = board[y_new][x_new]-10-1;
                    other_list = get_IDlist(Clients, ID_other);
                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x2=other_list->xp;
                    validity_move.new_y2=other_list->yp;
                    validity_move.other_ID=ID_other;

                    //superpowerpacman
                    //gets eaten
                    board[y_old][x_old] = 0;
                    elem=set_initialpos(board, n_cols,n_lines);
                    x_new =elem%n_cols;
                    y_new =elem/n_cols;


                    board[y_new][x_new] = -(ID + 10);
                    validity_move.flag2player = 4;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;

                }else {
                    //pacman
                    // eats it
                    ID_other = board[y_new][x_new] - 10;
                    board[y_old][x_old] = 0;
                    board[y_new][x_new] = -(10 + ID);
                    validity_move.flag2player = 1;
                    validity_move.character2 = 1;

                    elem = set_initialpos(board, n_cols, n_lines);

                    board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                    validity_move.new_x2 = elem % n_cols;
                    validity_move.new_y2 = elem / n_cols;
                    validity_move.other_ID=ID_other;
                    other_list = get_IDlist(Clients, ID_other);
                    other_list->xp = elem % n_cols;
                    other_list->yp = elem / n_cols;

                    validity_move.other_rgb_r = other_list->rgb_r;
                    validity_move.other_rgb_g = other_list->rgb_g;
                    validity_move.other_rgb_b = other_list->rgb_b;
                    validity_move.new_x1 = x_new;
                    validity_move.new_y1 = y_new;
                    validity_move.valid = 1;
                }
            } else if (x_new != x_old) {
                if (x_new > x_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(x_new - 2 >= 0) {
                            if (board[y_new][x_new - 2] == 0 ) {
                                validity_move.new_x1 = x_new - 2;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            } else if (board[y_new][x_new-2] == 4){
                                board[y_new][x_new-2] = -(10 + ID);
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;

                            }else if (board[y_new][x_new-2] == 5){
                                board[y_new][x_new-2] = -(10 + ID);
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            } else if (board[y_new][x_new-2] < -10) {
                                ID_other = (-10) - (board[y_new][x_new-2]);
                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = -(10 + ID);
                                validity_move.flag2player = 1;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.other_ID=ID_other;

                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm=x_old;
                                other_list->ym=y_old;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                                //monster of another player
                            } else if (board[y_new][x_new-2] == (ID + 10)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] == (ID + 10+1)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new][x_new-2];
                                board[y_new][x_new-2] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 2;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new-2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new-2] >10) {
                                if(board[y_new][x_new-2]%2==0){
                                    ID_other = board[y_new][x_new-2]-10-1;
                                    other_list = get_IDlist(Clients, ID_other);
                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x2=other_list->xp;
                                    validity_move.new_y2=other_list->yp;
                                    validity_move.other_ID=ID_other;

                                    //superpowerpacman
                                    //gets eaten
                                    board[y_old][x_old] = 0;
                                    elem=set_initialpos(board, n_cols,n_lines);
                                    x_new =elem%n_cols;
                                    y_new =elem/n_cols;


                                    board[y_new][x_new] = -(ID + 10);
                                    validity_move.flag2player = 4;
                                    validity_move.new_x1 = x_new;
                                    validity_move.new_y1 = y_new;
                                    validity_move.valid = 1;

                                }else {
                                    //pacman
                                    // eats it
                                    ID_other = board[y_new][x_new-2] - 10;
                                    board[y_old][x_old] = 0;
                                    board[y_new][x_new-2] = -(10 + ID);
                                    validity_move.flag2player = 1;
                                    validity_move.character2 = 1;

                                    elem = set_initialpos(board, n_cols, n_lines);

                                    board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                                    validity_move.new_x2 = elem % n_cols;
                                    validity_move.new_y2 = elem / n_cols;
                                    validity_move.other_ID=ID_other;
                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp = elem % n_cols;
                                    other_list->yp = elem / n_cols;

                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x1 = x_new-2;
                                    validity_move.new_y1 = y_new;
                                    validity_move.valid = 1;
                                }
                            }else {
                                validity_move.new_x1 = x_old;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_x1 = x_old;
                            validity_move.new_y1 = y_new;

                            board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                } else if (x_new < x_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(x_new + 2 <= n_cols - 1) {
                            if (board[y_new][x_new + 2] ==0) {
                                validity_move.new_x1 = x_new + 2;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;

                            }else if (board[y_new][x_new+2] == 4){
                                board[y_new][x_new+2] = -(10 + ID);
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;

                            }else if (board[y_new][x_new+2] == 5){
                                board[y_new][x_new+2] = -(10 + ID);
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            } else if (board[y_new][x_new+2] < -10) {
                                ID_other = (-10) - (board[y_new][x_new+2]);
                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = -(10 + ID);
                                validity_move.flag2player = 1;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.other_ID=ID_other;

                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm=x_old;
                                other_list->ym=y_old;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                                //monster of another player
                            } else if (board[y_new][x_new+2] == (ID + 10)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] == (ID + 10+1)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new][x_new+2];
                                board[y_new][x_new+2] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 2;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new+2;
                                validity_move.new_y1 = y_new;
                                validity_move.valid = 1;
                            }else if (board[y_new][x_new+2] >10) {
                                if(board[y_new][x_new+2]%2==0){
                                    ID_other = board[y_new][x_new+2]-10-1;
                                    other_list = get_IDlist(Clients, ID_other);
                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x2=other_list->xp;
                                    validity_move.new_y2=other_list->yp;
                                    validity_move.other_ID=ID_other;

                                    //superpowerpacman
                                    //gets eaten
                                    board[y_old][x_old] = 0;
                                    elem=set_initialpos(board, n_cols,n_lines);
                                    x_new =elem%n_cols;
                                    y_new =elem/n_cols;


                                    board[y_new][x_new] = -(ID + 10);
                                    validity_move.flag2player = 4;
                                    validity_move.new_x1 = x_new;
                                    validity_move.new_y1 = y_new;
                                    validity_move.valid = 1;

                                }else {
                                    //pacman
                                    // eats it
                                    ID_other = board[y_new][x_new+2] - 10;
                                    board[y_old][x_old] = 0;
                                    board[y_new][x_new+2] = -(10 + ID);
                                    validity_move.flag2player = 1;
                                    validity_move.character2 = 1;

                                    elem = set_initialpos(board, n_cols, n_lines);

                                    board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                                    validity_move.new_x2 = elem % n_cols;
                                    validity_move.new_y2 = elem / n_cols;
                                    validity_move.other_ID=ID_other;
                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp = elem % n_cols;
                                    other_list->yp = elem / n_cols;

                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x1 = x_new+2;
                                    validity_move.new_y1 = y_new;
                                    validity_move.valid = 1;
                                }
                            } else {
                                validity_move.new_x1 = x_old;
                                validity_move.new_y1 = y_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                validity_move.valid = 1;
                            }
                        } else {
                            validity_move.new_x1 = x_old;
                            validity_move.new_y1 = y_new;

                            board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                } else {
                    validity_move.new_y1 = y_new;
                    validity_move.new_x1 = x_new;
                    board[y_old][x_old] = 0;
                    board[y_new][x_new] = -(ID + 10);
                    validity_move.valid = 1;
                }
            } else if (y_new != y_old) {
                if (y_new > y_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(y_new - 2 >= 0) {
                            if (board[y_new - 2][x_new] == 0) {
                                validity_move.new_y1 = y_new - 2;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }  else if (board[y_new-2][x_new] == 4){
                                board[y_new-2][x_new] = -(10 + ID);
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;

                            }else if (board[y_new-2][x_new] == 5){
                                board[y_new-2][x_new] = -(10 + ID);
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            } else if (board[y_new-2][x_new] < -10) {
                                ID_other = (-10) - (board[y_new-2][x_new]);
                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = -(10 + ID);
                                validity_move.flag2player = 1;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.other_ID=ID_other;

                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm=x_old;
                                other_list->ym=y_old;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;
                                //monster of another player
                            } else if (board[y_new-2][x_new] == (ID + 10)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] == (ID + 10+1)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new-2][x_new];
                                board[y_new-2][x_new] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 2;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new-2;
                                validity_move.valid = 1;
                            }else if (board[y_new-2][x_new] >10) {
                                if(board[y_new-2][x_new]%2==0){
                                    ID_other = board[y_new-2][x_new]-10-1;
                                    other_list = get_IDlist(Clients, ID_other);
                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x2=other_list->xp;
                                    validity_move.new_y2=other_list->yp;
                                    validity_move.other_ID=ID_other;

                                    //superpowerpacman
                                    //gets eaten
                                    board[y_old][x_old] = 0;
                                    elem=set_initialpos(board, n_cols,n_lines);
                                    x_new =elem%n_cols;
                                    y_new =elem/n_cols;


                                    board[y_new][x_new] = -(ID + 10);
                                    validity_move.flag2player = 4;
                                    validity_move.new_x1 = x_new;
                                    validity_move.new_y1 = y_new;
                                    validity_move.valid = 1;

                                }else {
                                    //pacman
                                    // eats it
                                    ID_other = board[y_new-2][x_new] - 10;
                                    board[y_old][x_old] = 0;
                                    board[y_new-2][x_new] = -(10 + ID);
                                    validity_move.flag2player = 1;
                                    validity_move.character2 = 1;

                                    elem = set_initialpos(board, n_cols, n_lines);

                                    board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                                    validity_move.new_x2 = elem % n_cols;
                                    validity_move.new_y2 = elem / n_cols;
                                    validity_move.other_ID=ID_other;
                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp = elem % n_cols;
                                    other_list->yp = elem / n_cols;

                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x1 = x_new;
                                    validity_move.new_y1 = y_new-2;
                                    validity_move.valid = 1;
                                }
                            }else {
                                validity_move.new_y1 = y_old;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_y1 = y_old;
                            validity_move.new_x1 = x_new;

                            board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                } else if (y_new < y_old && board[y_new][x_new] != 0) {
                    if (board[y_new][x_new] == 1) {
                        if(y_new + 2 <= n_lines - 1) {
                            if (board[y_new + 2][x_new] == 0) {
                                validity_move.new_y1 = y_new + 2;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                board[y_old][x_old] = 0;
                                validity_move.valid = 1;
                            }   else if (board[y_new+2][x_new] == 4){
                                board[y_new+2][x_new] = -(10 + ID);
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 1;
                                validity_move.valid = 1;

                            }else if (board[y_new+2][x_new] == 5){
                                board[y_new+2][x_new] = -(10 + ID);
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                board[y_old][x_old]=0;

                                validity_move.flag_newfruit = 2;
                                validity_move.valid = 1;
                            } else if (board[y_new+2][x_new] < -10) {
                                ID_other = (-10) - (board[y_new+2][x_new]);
                                board[y_old][x_old] = board[y_new+2][x_new];
                                board[y_new+2][x_new] = -(10 + ID);
                                validity_move.flag2player = 1;
                                validity_move.character2 = 3;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.other_ID=ID_other;

                                other_list = get_IDlist(Clients, ID_other);
                                other_list->xm=x_old;
                                other_list->ym=y_old;

                                validity_move.other_rgb_r = other_list->rgb_r;
                                validity_move.other_rgb_g = other_list->rgb_g;
                                validity_move.other_rgb_b = other_list->rgb_b;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;
                                //monster of another player
                            } else if (board[y_new+2][x_new] == (ID + 10)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new+2][x_new];
                                board[y_new+2][x_new] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 1;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] == (ID + 10+1)) {
                                //the pacman of the same player

                                board[y_old][x_old] = board[y_new+2][x_new];
                                board[y_new+2][x_new] = -(ID + 10);
                                validity_move.flag2player = 2;
                                validity_move.character2 = 2;
                                validity_move.new_x2 = x_old;
                                validity_move.new_y2 = y_old;
                                validity_move.new_x1 = x_new;
                                validity_move.new_y1 = y_new+2;
                                validity_move.valid = 1;
                            }else if (board[y_new+2][x_new] >10) {
                                if(board[y_new+2][x_new]%2==0){
                                    ID_other = board[y_new+2][x_new]-10-1;
                                    other_list = get_IDlist(Clients, ID_other);
                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x2=other_list->xp;
                                    validity_move.new_y2=other_list->yp;
                                    validity_move.other_ID=ID_other;

                                    //superpowerpacman
                                    //gets eaten
                                    board[y_old][x_old] = 0;
                                    elem=set_initialpos(board, n_cols,n_lines);
                                    x_new =elem%n_cols;
                                    y_new =elem/n_cols;


                                    board[y_new][x_new] = -(ID + 10);
                                    validity_move.flag2player = 4;
                                    validity_move.new_x1 = x_new;
                                    validity_move.new_y1 = y_new;
                                    validity_move.valid = 1;

                                }else {
                                    //pacman
                                    // eats it
                                    ID_other = board[y_new+2][x_new] - 10;
                                    board[y_old][x_old] = 0;
                                    board[y_new+2][x_new] = -(10 + ID);
                                    validity_move.flag2player = 1;
                                    validity_move.character2 = 1;

                                    elem = set_initialpos(board, n_cols, n_lines);

                                    board[elem / n_cols][elem % n_cols] = (10 + ID_other);
                                    validity_move.new_x2 = elem % n_cols;
                                    validity_move.new_y2 = elem / n_cols;
                                    validity_move.other_ID=ID_other;
                                    other_list = get_IDlist(Clients, ID_other);
                                    other_list->xp = elem % n_cols;
                                    other_list->yp = elem / n_cols;

                                    validity_move.other_rgb_r = other_list->rgb_r;
                                    validity_move.other_rgb_g = other_list->rgb_g;
                                    validity_move.other_rgb_b = other_list->rgb_b;
                                    validity_move.new_x1 = x_new;
                                    validity_move.new_y1 = y_new+2;
                                    validity_move.valid = 1;
                                }
                            }else {
                                validity_move.new_y1 = y_old;
                                validity_move.new_x1 = x_new;

                                board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                                validity_move.valid = 1;
                            }
                        }else {
                            validity_move.new_y1 = y_old;
                            validity_move.new_x1 = x_new;

                            board[validity_move.new_y1][validity_move.new_x1] = -(ID + 10);
                            validity_move.valid = 1;
                        }
                    }
                } else {
                    validity_move.new_y1 = y_new;
                    validity_move.new_x1 = x_new;
                    board[y_new][x_new] = -(ID + 10);
                    board[y_old][x_old] = 0;
                    validity_move.valid = 1;
                }
            }
            return validity_move;
        }
    }
    validity_move.valid = 0;
    return validity_move;
}


//----------------------------------------------------------//
//Thread responsible for receiving messages from each player//
//----------------and deal with initial protocol------------//


void * clientThreadi(void * arg){
    struct threadclient_arg *my_arg = (struct threadclient_arg *)arg;
    int sock_fd=(*my_arg).sock_fd;
    int ID=(*my_arg).ID;
    int i,j;

    //----------------------------------------------------------------------------------------------------------//
    //------------------------------------receiving the colour of the character---------------------------------//
    //----------------------------------------------------------------------------------------------------------//
    message m;
    int err_rcv;
    err_rcv = recv(sock_fd, &m , sizeof(m), 0);
    if (err_rcv<0){
        perror("Error in reading: ");
        exit(-1);
    }


    colour_msg msg_colour;
    err_rcv = recv(sock_fd, &msg_colour , m.size, 0);
    if (err_rcv<0){
        perror("Error in reading: ");
        exit(-1);
    }
    int rgb_r=msg_colour.r;
    int rgb_g=msg_colour.g;
    int rgb_b=msg_colour.b;
    //----------------------------------------------------------------------------------------------------------//
    //------------------------------sending board size and sending initial position-----------------------------//
    //----------------------------------------------------------------------------------------------------------//

    initialinfo_msg con_msg;
    con_msg.n_lin=n_lines;
    con_msg.n_col=n_cols;
    con_msg.ID=ID;



    //sync needed dont use board directly
    int elem;
    int elem1=set_initialpos(board, n_cols,n_lines);
    if(elem1<0){
        printf("board is full \n");
        close(sock_fd);
        pthread_exit(NULL);
        //close thread and etc
    }
    con_msg.xpac_ini=elem1%n_cols;
    con_msg.ypac_ini=elem1/n_cols;
    board[elem1/n_cols][elem1%n_cols]=ID+10;

    printf("ola\n");
    int elem2=set_initialpos(board, n_cols,n_lines);
    if(elem2<0){
        printf("board is full \n");
        board[elem1/n_cols][elem1%n_cols]=0;
        close(sock_fd);
        pthread_exit(NULL);
        //close thread and etc
    }
    printf("ola\n");
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
    for ( i = 0 ; i <  con_msg.n_lin; i++){
        printf("%2d ", i);
        for ( j = 0 ; j <  con_msg.n_col; j++) {
            printf("%2d ", send_board[i*(con_msg.n_col)+j]);
        }
        printf("\n");
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
    i=0;
    send_list=Clients;
    while(send_list!=NULL){
        send_list_array[i]=send_list->ID;
        send_list_array[i+1]=send_list->rgb_r;
        send_list_array[i+2]=send_list->rgb_g;
        send_list_array[i+3]=send_list->rgb_b;
        i++;
        send_list=get_next_ID(send_list);
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
    event_data_ini->ym= con_msg.ym_ini;
    event_data_ini->r = rgb_r;
    event_data_ini->g = rgb_g;
    event_data_ini->b = rgb_b;

    SDL_zero(new_event);
    new_event.type = Event_ShownewCharacter;
    new_event.user.data1 = event_data_ini;
    SDL_PushEvent(&new_event);



    //----------------------------------------------------------------------------------------------------------//
    //---------------------------------------------Insert new client info---------------------------------------//
    //----------------------------------------------------------------------------------------------------------//
    IDList * this_client = (IDList *) malloc(sizeof(IDList));
    if(this_client == NULL){
        return NULL;
    }
    this_client->ID = ID;
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
    if(Clients!=NULL){

        Event_ShownewFruits_Data *event_data_newfruit;
        event_data_newfruit = malloc(sizeof(Event_ShownewFruits_Data));

        pthread_t thr_fruit1,thr_fruit2;
        event_data_newfruit->fruit1=0;
        event_data_newfruit->fruit2=0;

        elem=set_initialpos(board, n_cols,n_lines);
        if(elem<0){
            printf("board is full \n");
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

            error = pthread_create(&thr_fruit1,NULL,thr_placefruit, Cherry_list);
        }


        elem=set_initialpos(board, n_cols,n_lines);
        if(elem<0){
            printf("board is full \n");
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

            Fruits=insert_new_Fruit(Fruits,Lemon_list );
            sem_init(&Lemon_list->sem_fruit1,0,0);
            sem_init(&Lemon_list->sem_fruit2,0,0);

            error = pthread_create(&thr_fruit2,NULL,thr_placefruit, Lemon_list);
        }

        SDL_zero(new_event);
        new_event.type = Event_ShownewFruits;
        new_event.user.data1 = event_data_newfruit;
        SDL_PushEvent(&new_event);
    }


    //need to make sync here
    Clients=insert_new_ID(Clients, this_client);


    //-------------------------------------------------------//
    //------Initialize the variables for the game moves------//
    //-------------------------------------------------------//
    int x_new,y_new,character,flag_2player=0;
    send_character_pos_msg msg_pos;


    Event_ShowCharacter_Data *event_data;
    event_data = malloc(sizeof(Event_ShowCharacter_Data));

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
    error = pthread_create(&thr_nplayspersec1,NULL,thr_forplayspersec , &arg_sem1);

    struct sem_nplayspersec arg_sem3;
    pthread_t thr_nplayspersec3;
    sem_init(&arg_sem3.sem_nplay1, 0, 0);
    sem_init(&arg_sem3.sem_nplay2, 0, 1);
    error = pthread_create(&thr_nplayspersec3,NULL,thr_forplayspersec , &arg_sem3);

    pthread_t thr_ina1;
    struct sem_inactivity inactivity1;
    sem_init(&inactivity1.sem_inactivity,0,0);
    inactivity1.ID=ID;
    inactivity1.character=1;
    inactivity1.rgb_r=rgb_r;
    inactivity1.rgb_g=rgb_g;
    inactivity1.rgb_b=rgb_b;
    error = pthread_create(&thr_ina1,NULL,thr_inactivity, &inactivity1);

    pthread_t thr_ina3;
    struct sem_inactivity inactivity3;
    sem_init(&inactivity3.sem_inactivity,0,0);
    inactivity3.ID=ID;
    inactivity3.character=3;
    inactivity3.rgb_r=rgb_r;
    inactivity3.rgb_g=rgb_g;
    inactivity3.rgb_b=rgb_b;
    error = pthread_create(&thr_ina3,NULL,thr_inactivity, &inactivity3);

    int x_old;
    int y_old;
    int superpower=0;
    IDList * other_client = (IDList *) malloc(sizeof(IDList));
    int ID_other;

    //-------------------------------------------------------//
    //---------read the moves made by the client-------------//
    //----loop receiving updated positions  from socket------//
    //-------------------------------------------------------//

    while((err_rcv=recv(sock_fd, &m, sizeof(m),0))>0){

        if(m.action==9){

            //NEED SYNC HERE
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

                Cherry_list = get_fruit_list(Fruits, 4);
                Lemon_list = get_fruit_list(Fruits, 5);
                event_data_cleanfruit->fruit1=0;
                event_data_cleanfruit->fruit2=0;
                if(Cherry_list!=NULL){
                    board[Cherry_list->y][Cherry_list->x] = 0;
                    event_data_cleanfruit->x1=Cherry_list->x;
                    event_data_cleanfruit->y1=Cherry_list->y;
                    event_data_cleanfruit->fruit1=4;
                    Fruits =remove_fruit_list(Fruits, Cherry_list);
                }

                if(Lemon_list!=NULL){
                    board[Lemon_list->y][Lemon_list->x] = 0;
                    event_data_cleanfruit->x2=Lemon_list->x;
                    event_data_cleanfruit->y2=Lemon_list->y;
                    event_data_cleanfruit->fruit2=5;
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

            //clear semaphores
            error=sem_destroy(&inactivity1.sem_inactivity);
            sem_destroy(&inactivity3.sem_inactivity);
            sem_destroy(&arg_sem1.sem_nplay1);
            sem_destroy(&arg_sem1.sem_nplay2);
            sem_destroy(&arg_sem3.sem_nplay1);
            sem_destroy(&arg_sem3.sem_nplay2);
            pthread_join(thr_ina1, NULL);
            pthread_join(thr_ina3, NULL);
            pthread_join(thr_nplayspersec1, NULL);
            pthread_join(thr_nplayspersec3, NULL);

            //clear socket
            close(sock_fd);
            printf("%d %d\n", n_cols, n_lines);
            for ( i = 0 ; i < n_lines; i++){
                printf("%2d ", i);
                for ( j = 0 ; j < n_cols; j++) {
                    printf("%d", board[i][j]);
                }
                printf("\n");
            }
            pthread_exit(NULL);
        }


        err_rcv = recv(sock_fd, &msg_pos, sizeof(msg_pos), 0);
        printf("cycle receiving %d byte %d %d %d\n", err_rcv, msg_pos.character, msg_pos.x, msg_pos.y);
        x_new = msg_pos.x;
        y_new = msg_pos.y;
        character = msg_pos.character;

        // get old position NEED SYNC
        if (character == 1) {
            x_old = this_client->xp;
            y_old = this_client->yp;
            printf("old_p x: %d y: %d \n", x_old, y_old);
        } else if (character == 3) {
            x_old = this_client->xm;
            y_old = this_client->ym;
            superpower = this_client->superpower;
            printf("old_m x: %d y: %d \n", x_old, y_old);
        } else {
            continue;
        }

        if (x_new == x_old && y_new == y_old) {
            continue;
        }else{
            if(character==1){

                sem_getvalue(&arg_sem1.sem_nplay2, &sem_value_nplaypersec);
                if (sem_value_nplaypersec == 0) {
                    continue;
                } else {
                    if(this_client->superpower>0){
                        character=2;
                    }
                    validity_move = check_move(Clients, x_new, y_new, x_old, y_old, character, ID, this_client->superpower);

                    if (validity_move.valid == 1 && (validity_move.character1 == 1 || validity_move.character1 == 2)) {
                        this_client->xp = validity_move.new_x1;
                        this_client->yp = validity_move.new_y1;
                    } else if (validity_move.valid == 1 && validity_move.flag2player == 2 &&
                               (validity_move.character2 == 1 || validity_move.character2 == 2)) {
                        this_client->xp = validity_move.new_x2;
                        this_client->yp = validity_move.new_y2;
                    }
                    if (validity_move.valid == 1 && validity_move.character1 == 3) {
                        this_client->xm = validity_move.new_x1;
                        this_client->ym = validity_move.new_y1;
                    } else if (validity_move.valid == 1 && validity_move.character2 == 3 &&
                               validity_move.flag2player == 2) {
                        this_client->xm = validity_move.new_x2;
                        this_client->ym = validity_move.new_y2;
                    }
                    printf("xp: %d yp: %d valid: %d\n", this_client->xp, this_client->yp, validity_move.valid);
                    printf("xp: %d yp: %d valid: %d\n", this_client->xm, this_client->ym, validity_move.valid);

                    printf("wtf is happening %d\n",validity_move.flag_newfruit);
                    if(validity_move.valid == 1 && validity_move.flag_newfruit == 1){

                        this_client->superpower=1;

                        Cherry_list = get_fruit_list(Fruits,4);
                        sem_post(&Cherry_list->sem_fruit1);

                        this_client->score++;

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

                        this_client->superpower=1;
                        Lemon_list = get_fruit_list(Fruits,5);
                        sem_post(&Lemon_list->sem_fruit1);

                        this_client->score++;

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
                        printf("flag2player: %d\n", flag_2player);
                        if (flag_2player == 0) {

                            if(validity_move.character2==3) {
                                ID_other = (-board[validity_move.new_y2][validity_move.new_x2])-10;
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
                        }
                        if (flag_2player == 1 || flag_2player == 3) {
                            if(flag_2player==3){
                                // superpacman eats a monster
                                this_client->superpower++;
                                if (this_client->superpower>=3){
                                    this_client->superpower=0;
                                    validity_move.character1=1;
                                }
                                this_client->score++;

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
                            event_data2player->r2 = validity_move.other_rgb_r;
                            event_data2player->g2 = validity_move.other_rgb_g;
                            event_data2player->b2 = validity_move.other_rgb_b;
                            event_data2player->ID1=ID;
                            event_data2player->ID2=validity_move.other_ID;

                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_Change2Characters;
                            new_event.user.data1 = event_data2player;
                            SDL_PushEvent(&new_event);
                            flag_2player = 0;
                        }
                        if (flag_2player == 2) {
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
                    continue;
                } else {
                    validity_move = check_move(Clients, x_new, y_new, x_old, y_old, character, ID,superpower);

                    if (validity_move.valid == 1 && (validity_move.character1 == 1 || validity_move.character1 == 2)) {
                        this_client->xp = validity_move.new_x1;
                        this_client->yp = validity_move.new_y1;
                    } else if (validity_move.valid == 1 && validity_move.flag2player == 2 &&
                               (validity_move.character2 == 1 || validity_move.character2 == 2)) {
                        this_client->xp = validity_move.new_x2;
                        this_client->yp = validity_move.new_y2;
                    }
                    if (validity_move.valid == 1 && validity_move.character1 == 3) {
                        this_client->xm = validity_move.new_x1;
                        this_client->ym = validity_move.new_y1;
                    } else if (validity_move.valid == 1 && validity_move.character2 == 3 &&
                               validity_move.flag2player == 2) {
                        this_client->xm = validity_move.new_x2;
                        this_client->ym = validity_move.new_y2;
                    }
                    printf("xp: %d yp: %d valid: %d\n", this_client->xp, this_client->yp, validity_move.valid);
                    printf("xp: %d yp: %d valid: %d\n", this_client->xm, this_client->ym, validity_move.valid);
                    //get old position
                    //board[y_oldp][x_oldp]=0;
                    // here see if play is legal
                    // check if it is the 4 adjacent places (down, up,left,right)

                    // check if it going against a brick or the limit of the board it will be bounced back
                    //if the character cannot be bounced back it will stay at the same place
                    printf("fruit id: %d\n", validity_move.flag_newfruit);
                    if(validity_move.valid == 1 && validity_move.flag_newfruit == 1){
                        Cherry_list = get_fruit_list(Fruits,4);
                        sem_post(&Cherry_list->sem_fruit1);

                        this_client->score++;

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
                        printf("flag2player: %d\n", flag_2player);
                        if (flag_2player == 0 || flag_2player == 4) {
                            if(flag_2player==4){
                                // monster gets eaten

                                ID_other = board[validity_move.new_y2][validity_move.new_x2]-10-1;
                                other_client = get_IDlist(Clients, ID_other);
                                other_client->score++;
                                other_client->superpower++;

                                //superpowerpacman
                                //gets eaten

                                printf("svddgv %d\n",other_client->superpower);
                                if (other_client->superpower>=3){
                                    other_client->superpower=0;


                                    board[validity_move.new_y2][validity_move.new_x2]=board[validity_move.new_y2][validity_move.new_x2]-1;

                                    event_data->character = 1;
                                    event_data->x = validity_move.new_x2;
                                    event_data->y = validity_move.new_y2;
                                    event_data->x_old = validity_move.new_x2;
                                    event_data->y_old = validity_move.new_y2;
                                    event_data->r = validity_move.other_rgb_r;
                                    event_data->g = validity_move.other_rgb_g;
                                    event_data->b = validity_move.other_rgb_b;
                                    event_data->ID=ID_other;

                                    // send the event
                                    SDL_zero(new_event);
                                    new_event.type = Event_ShowCharacter;
                                    new_event.user.data1 = event_data;
                                    SDL_PushEvent(&new_event);
                                }
                            }

                            event_data = malloc(sizeof(Event_ShowCharacter_Data));
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

                            if(validity_move.character2==1){
                                this_client->score++;
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
                            event_data2player->r2 = validity_move.other_rgb_r;
                            event_data2player->g2 = validity_move.other_rgb_g;
                            event_data2player->b2 = validity_move.other_rgb_b;
                            event_data2player->ID1=ID;
                            event_data2player->ID2=validity_move.other_ID;
                            printf("%d %d\n",ID,validity_move.other_ID);

                            // send the event
                            SDL_zero(new_event);
                            new_event.type = Event_Change2Characters;
                            new_event.user.data1 = event_data2player;
                            SDL_PushEvent(&new_event);
                            flag_2player = 0;
                        }
                        if (flag_2player == 2) {
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

    }

    //NEED SYNC HERE
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

        Cherry_list = get_fruit_list(Fruits, 4);
        Lemon_list = get_fruit_list(Fruits, 5);
        event_data_cleanfruit->fruit1=0;
        event_data_cleanfruit->fruit2=0;
        if(Cherry_list!=NULL){
            board[Cherry_list->y][Cherry_list->x] = 0;
            event_data_cleanfruit->x1=Cherry_list->x;
            event_data_cleanfruit->y1=Cherry_list->y;
            event_data_cleanfruit->fruit1=4;
            Fruits =remove_fruit_list(Fruits, Cherry_list);
        }

        if(Lemon_list!=NULL){
            board[Lemon_list->y][Lemon_list->x] = 0;
            event_data_cleanfruit->x2=Lemon_list->x;
            event_data_cleanfruit->y2=Lemon_list->y;
            event_data_cleanfruit->fruit2=5;
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

    //clear semaphores
    error=sem_destroy(&inactivity1.sem_inactivity);
    sem_destroy(&inactivity3.sem_inactivity);
    sem_destroy(&arg_sem1.sem_nplay1);
    sem_destroy(&arg_sem1.sem_nplay2);
    sem_destroy(&arg_sem3.sem_nplay1);
    sem_destroy(&arg_sem3.sem_nplay2);
    pthread_join(thr_ina1, NULL);
    pthread_join(thr_ina3, NULL);
    pthread_join(thr_nplayspersec1, NULL);
    pthread_join(thr_nplayspersec3, NULL);

    //clear socket
    close(sock_fd);
    printf("%d %d\n", n_cols, n_lines);
    for ( i = 0 ; i < n_lines; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < n_cols; j++) {
            printf("%d", board[i][j]);
        }
        printf("\n");
    }
    pthread_exit(NULL);
}

//------------------------------------------------//
// ----------Thread for accepting connection------//
//------------------------------------------------//

int server_socket;
void * thread_Accept(void * argc){
    int ID=1;
    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(client_addr);
    int sock_fd;
    struct threadclient_arg *arg;

    pthread_t clientthread_id;
    while(1){
        printf("waiting for connections\n");
        sock_fd= accept(server_socket,(struct sockaddr *) & client_addr, &size_addr);
        if(sock_fd == -1) {
            perror("accept");
            exit(-1);
        }
        arg = malloc(sizeof(struct threadclient_arg));
        (*arg).sock_fd=sock_fd;
        (*arg).ID=ID;
        pthread_create(&clientthread_id, NULL, clientThreadi, (void*)arg);
        printf("accepted connection \n");
        ID=ID+2;
    }
    return (NULL);
}


void * send_scores(void * argc){
    IDList *aux;
    IDList * aux2;
    int size=0,i;
    score_msg=malloc(sizeof(int)*2);
    message m;
    m.action=8;



    while(1){


        sleep(2);

        if(Clients==NULL){
            continue;

        }else{

            if(Clients->next==NULL){

                Clients->score=0;
                printf("Client ID: %d score: %d \n\n", Clients->ID, Clients->score);
            } else {

                aux=Clients;
                aux2=Clients;
                size=0;
                while (aux != NULL) {
                    size++;
                    aux = get_next_ID(aux);
                }
                m.size= sizeof(int)*2*size;
                score_msg=realloc(score_msg,m.size);
                aux=aux2;
                i=0;
                while (aux != NULL) {
                    score_msg[i*2]=aux->ID;
                    score_msg[i*2+1]=aux->score;
                    i++;
                    aux = get_next_ID(aux);
                }
                aux=aux2;
                while (aux != NULL) {
                    printf("Client ID: %d score: %d \n", aux->ID, aux->score);
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, score_msg, m.size, 0);
                    aux = aux->next;

                }
            }
        }

    }
    return (NULL);
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
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1){
        perror("socket: ");
        exit(-1);
    }
    server_local_addr.sin_family = AF_INET;
    server_local_addr.sin_addr.s_addr = INADDR_ANY;
    server_local_addr.sin_port = htons(3000);
    err = bind(server_socket, (struct sockaddr *)&server_local_addr,sizeof(server_local_addr));
    if(err == -1) {
        perror("bind");
        exit(-1);
    }
    if(listen(server_socket, 20) == -1) {
        perror("listen");
        exit(-1);
    }

    pthread_create(&thread_Accept_id, NULL, thread_Accept, NULL);
    pthread_create(&thread_score, NULL, send_scores, NULL);



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
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
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
                printf("%d\n", data->character);
                msg.x1 = data->x;
                msg.y1 = data->x;
                msg.x2 = data->x_old;
                msg.y2 = data->y_old;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;
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
                IDList *aux = Clients;

                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
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
                IDList *aux = Clients;

                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg_fruit, m.size, 0);
                    aux = aux->next;
                }
            }
            if (event.type == Event_ShownewFruits) {
                rec_object_msg msg_fruit1;
                rec_object_msg msg_fruit2;

                message m;
                m.action=3;
                m.size= sizeof(msg_fruit1);
                // we get the data (created with the malloc)
                Event_ShownewFruits_Data *data = event.user.data1;
                paint_cherry(data->x1,data->y1);
                paint_lemon(data->x2,data->y2);
                printf("fruit in -%d y-%d\n", data->x1, data->y1);
                printf("fruit in -%d y-%d\n", data->x2, data->y2);

                msg_fruit1.object=4;
                msg_fruit1.x=data->x1;
                msg_fruit1.y=data->y1;

                msg_fruit2.object=5;
                msg_fruit2.x=data->x2;
                msg_fruit2.y=data->y2;

                //SYNC MECHANISM
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
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;

                }
            }

            if (event.type == Event_Change2Characters){
                rec_change2characters_msg msg_c2c;
                message m;
                m.action=5;
                m.size= sizeof(msg_c2c);
                // we get the data (created with the malloc)
                Event_Change2Characters_Data *data = event.user.data1;
                //printf("ola2 dvsf %d %d %d \n", data->character1,data->x1,data->y1);

                clear_place(data->x1_old, data->y1_old);
                if (data->character1 == 1) {
                    paint_pacman(data->x1, data->y1, data->r1, data->g1, data->b1);
                } else if (data->character1 == 2) {
                    paint_powerpacman(data->x1, data->y1, data->r1, data->g1, data->b1);
                } else if (data->character1 == 3) {
                    paint_monster(data->x1, data->y1, data->r1, data->g1, data->b1);
                }
                //printf("ola2 dvsf %d %d  %d\n", data->character2,data->x2,data->y2);
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
                printf("%d %d\n",msg_c2c.ID1,msg_c2c.ID2);

                //SYNC MECHANISM
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg_c2c, m.size, 0);
                    aux = aux->next;

                }

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
                IDList *aux = Clients;
                while (aux != NULL) {
                    send(aux->socket, &m, sizeof(message),0);
                    send(aux->socket, &msg, m.size, 0);
                    aux = aux->next;
                }
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
            }
        }
    }

    printf("fim\n");
    close_board_windows();
    exit(0);
}
