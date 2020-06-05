#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>

//gcc teste.c UI_library.c -o teste-UI -lSDL2 -lSDL2_image
#include "com_protocol.h"
#include "UI_library.h"



Uint32 Event_ShowPacman;
Uint32 Event_ShownewPlayer;
Uint32 Event_ShowFruit;
Uint32 Event_CleanFruit;
Uint32 Event_ShowSuperPacman;
Uint32 Event_ShowMonster;
Uint32 Event_Change2Player;
Uint32 Event_DeletePlayer;

typedef struct Event_ShownewPlayer_Data{
    int ID,xp,yp,xm,ym,r,g,b;
} Event_ShownewPlayer_Data;

typedef struct Event_DeletePlayer_Data{
    int xp,yp,xm,ym;
} Event_DeletePlayer_Data;

typedef struct Event_ShowCharacter_Data{
    int character,ID,x,y,x_old,y_old,r,g,b;
} Event_ShowCharacter_Data;

typedef struct Event_Change2Player_Data{
    int character1,ID1,x1,y1,x1_old,y1_old,r1,g1,b1,character2,ID2,x2,y2,r2,g2,b2;
} Event_Change2Player_Data;

typedef struct Event_ShowObject_Data{
    int object,x,y;
} Event_ShowObject_Data;






int sock_fd;
int ID;
void * socketThread(void * arg){
    int i;

    SDL_Event new_event;
    Event_ShowCharacter_Data * event_data;
    Event_ShownewPlayer_Data * event_data_new;
    Event_ShowObject_Data * event_data_fruit;
    Event_Change2Player_Data * event_data_c2c;
    Event_DeletePlayer_Data * event_data_delete;

    message m;
    rec_character_pos_msg msg_pos;
    rec_object_msg msg_obj;
    rec_change2characters_msg msg_c2c;
    int err_rcv;


    //loop receiving updated positions  from socket
    while((err_rcv=recv(sock_fd, &m, sizeof(message),0))>0){


        if(m.action==2) {


            err_rcv = recv(sock_fd, &msg_pos, m.size, 0);


            if (msg_pos.character == 1) {
                event_data = malloc(sizeof(Event_ShowCharacter_Data));
                event_data->character = msg_pos.character;
                event_data->x = msg_pos.x1;
                event_data->y = msg_pos.y1;
                event_data->x_old = msg_pos.x2;
                event_data->y_old = msg_pos.y2;
                event_data->r = msg_pos.r;
                event_data->g = msg_pos.g;
                event_data->b = msg_pos.b;
                event_data->ID= msg_pos.ID;


                // send the event
                SDL_zero(new_event);
                new_event.type = Event_ShowPacman;
                new_event.user.data1 = event_data;
                SDL_PushEvent(&new_event);
            }

            if (msg_pos.character == 2) {
                event_data = malloc(sizeof(Event_ShowCharacter_Data));
                event_data->character = msg_pos.character;
                event_data->x = msg_pos.x1;
                event_data->y = msg_pos.y1;
                event_data->x_old = msg_pos.x2;
                event_data->y_old = msg_pos.y2;
                event_data->r = msg_pos.r;
                event_data->g = msg_pos.g;
                event_data->b = msg_pos.b;
                event_data->ID=msg_pos.ID;

                //  send the event
                SDL_zero(new_event);
                new_event.type = Event_ShowSuperPacman;
                new_event.user.data1 = event_data;
                SDL_PushEvent(&new_event);
            }

            if (msg_pos.character == 3) {
                event_data = malloc(sizeof(Event_ShowCharacter_Data));
                event_data->character = msg_pos.character;
                event_data->x = msg_pos.x1;
                event_data->y = msg_pos.y1;
                event_data->x_old = msg_pos.x2;
                event_data->y_old = msg_pos.y2;
                event_data->r = msg_pos.r;
                event_data->g = msg_pos.g;
                event_data->b = msg_pos.b;
                event_data->ID=msg_pos.ID;

                // send the event
                SDL_zero(new_event);
                new_event.type = Event_ShowMonster;
                new_event.user.data1 = event_data;
                SDL_PushEvent(&new_event);
            }
        }else if(m.action==3){
            printf("--------------the paint a fruit--------------\n");

            err_rcv = recv(sock_fd, &msg_obj, m.size, 0);


            event_data_fruit = malloc(sizeof(Event_ShowObject_Data));
            event_data_fruit->object = msg_obj.object;
            event_data_fruit->x =  msg_obj.x;
            event_data_fruit->y  = msg_obj.y;


            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowFruit;
            new_event.user.data1 = event_data_fruit;
            SDL_PushEvent(&new_event);
        }else if(m.action==4){

            err_rcv = recv(sock_fd, &msg_pos, m.size, 0);


            event_data_new = malloc(sizeof(Event_ShownewPlayer_Data));
            event_data_new->xp = msg_pos.x1;
            event_data_new->yp = msg_pos.y1;
            event_data_new->xm = msg_pos.x2;
            event_data_new->ym = msg_pos.y2;
            event_data_new->r = msg_pos.r;
            event_data_new->g = msg_pos.g;
            event_data_new->b = msg_pos.b;
            event_data_new->ID=msg_pos.ID;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShownewPlayer;
            new_event.user.data1 = event_data_new;
            SDL_PushEvent(&new_event);
        }else if(m.action==5){
            err_rcv = recv(sock_fd, &msg_c2c, m.size, 0);

            printf("--------------change 2 player--------------\n");
            event_data_c2c = malloc(sizeof(Event_Change2Player_Data));

            event_data_c2c->character1 = msg_c2c.character1;
            event_data_c2c->x1_old = msg_c2c.x1_old;
            event_data_c2c->y1_old = msg_c2c.y1_old;
            event_data_c2c->x1=msg_c2c.x1;
            event_data_c2c->y1 = msg_c2c.y1;
            event_data_c2c->r1= msg_c2c.r1;
            event_data_c2c->g1 = msg_c2c.g1;
            event_data_c2c->b1 = msg_c2c.b1;
            event_data_c2c->ID1=msg_c2c.ID1;

            event_data_c2c->character2 = msg_c2c.character2;
            event_data_c2c->x2 = msg_c2c.x2;
            event_data_c2c->y2 = msg_c2c.y2;
            event_data_c2c->r2= msg_c2c.r2;
            event_data_c2c->g2 = msg_c2c.g2;
            event_data_c2c->b2 = msg_c2c.b2;
            event_data_c2c->ID2=msg_c2c.ID2;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_Change2Player;
            new_event.user.data1 = event_data_c2c;
            SDL_PushEvent(&new_event);

        }else if(m.action==6){
            err_rcv = recv(sock_fd, &msg_pos, m.size, 0);


            printf("--------------other player disconnect--------------\n");
            event_data_delete = malloc(sizeof(Event_ShownewPlayer_Data));
            event_data_delete->xp = msg_pos.x1;
            event_data_delete->yp = msg_pos.y1;
            event_data_delete->xm = msg_pos.x2;
            event_data_delete->ym = msg_pos.y2;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_DeletePlayer;
            new_event.user.data1 = event_data_delete;
            SDL_PushEvent(&new_event);


        }else if(m.action==7){
            err_rcv = recv(sock_fd, &msg_obj, m.size, 0);

            printf("--------------clean 1 fruits--------------\n");
            event_data_fruit = malloc(sizeof(Event_ShowObject_Data));
            event_data_fruit->object = msg_obj.object;
            event_data_fruit->x =  msg_obj.x;
            event_data_fruit->y  = msg_obj.y;


            // send the event
            SDL_zero(new_event);
            new_event.type = Event_CleanFruit;
            new_event.user.data1 = event_data_fruit;
            SDL_PushEvent(&new_event);



        }else if(m.action==8){

            printf("\n\n");
            printf("------------------score--------------------\n");
            score_msg = (int *)malloc(m.size);
            err_rcv = recv(sock_fd, score_msg, m.size, 0);

            for(i=0;i<m.size/8;i++){

                if(score_msg[i*2]==ID){

                    printf("-------------------------------------------\n");
                    printf("---------------YOUR SCORE------------------\n");
                    printf("         Player ID: %d, score: %d\n",score_msg[i*2],score_msg[i*2+1]);
                    printf("-------------------------------------------\n");
                }else{
                    printf("Player ID: %d, score: %d\n",score_msg[i*2],score_msg[i*2+1]);
                }
            }
            printf("\n\n");
        }else {
            continue;
        }
    }
    printf("fim\n");
    exit(0);

}





int main(int argc, char* argv[]){

    if(argc!=6){
        exit(-1);
    }
    SDL_Event event;
    int done = 0;

    Event_ShowPacman       = SDL_RegisterEvents(1);
    Event_ShownewPlayer    = SDL_RegisterEvents(1);
    Event_ShowSuperPacman  = SDL_RegisterEvents(1);
    Event_ShowMonster      = SDL_RegisterEvents(1);
    Event_DeletePlayer     = SDL_RegisterEvents(1);
    Event_Change2Player    = SDL_RegisterEvents(1);
    Event_CleanFruit       = SDL_RegisterEvents(1);
    Event_ShowFruit        = SDL_RegisterEvents(1);


    /*---------------------------------------------*/
    // receiving colour for characters //
    int rgb_r,rgb_g,rgb_b;
    if(sscanf(argv[3], "%d",&rgb_r)!=1){
        printf("arg[3] is not a number \n");
        exit(-1);
    }
    if(sscanf(argv[4], "%d",&rgb_g)!=1){
        printf("arg[4] is not a number \n");
        exit(-1);
    }
    if(sscanf(argv[5], "%d",&rgb_b)!=1){
        printf("arg[5] is not a number \n");
        exit(-1);
    }
    /*---------------------------------------------*/

    // creating socket and connecting to server //

    int port_number;
    if(sscanf(argv[2], "%d",&port_number)!=1){
        printf("arg[2] is not a number \n");
        exit(-1);
    }

    connection_data  con_data;
    con_data = pacman_connection(argv[1],port_number,rgb_r,rgb_g,rgb_b);
    if (con_data.error==-1){
        printf("Error in connection \n");
        exit(-1);
    }

    sock_fd=con_data.sock_fd;
    ID=con_data.ID;
    pthread_t socketThread_id;
    // create thread to receive messages//
    pthread_create(&socketThread_id, NULL, socketThread,NULL);



    //monster and packman position
    int x_pacman=con_data.xp_ini;
    int y_pacman=con_data.yp_ini;
    int x_monster=con_data.xm_ini;
    int y_monster=con_data.ym_ini;
    int x_monster_send=0;
    int y_monster_send=0;


    while (!done){
        while (SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                done = SDL_TRUE;
            }
            //when the mouse mooves the monster also moves
            if(event.type == SDL_MOUSEMOTION){

                //this fucntion return the place cwher the mouse cursor is
                get_board_place(event.motion.x, event.motion.y,&x_pacman, &y_pacman);
                pacman_sendcharacter_data(sock_fd,ID, x_pacman, y_pacman, 1);
            }
            if(event.type == SDL_KEYDOWN) {

                if (event.key.keysym.sym == SDLK_w ) {
                    pacman_disconnect(sock_fd);
                    printf("fim\n");
                    close_board_windows();
                    exit(0);


                }
                if (event.key.keysym.sym == SDLK_RIGHT ){

                    x_monster_send=x_monster+1;
                    y_monster_send=y_monster;
                    pacman_sendcharacter_data(sock_fd,ID, x_monster_send, y_monster_send, 3);

                }
                if (event.key.keysym.sym == SDLK_LEFT ){

                    x_monster_send=x_monster-1;
                    y_monster_send=y_monster;
                    pacman_sendcharacter_data(sock_fd,ID, x_monster_send, y_monster_send, 3);
                }
                if (event.key.keysym.sym == SDLK_UP ){

                    x_monster_send=x_monster;
                    y_monster_send=y_monster-1;
                    pacman_sendcharacter_data(sock_fd,ID, x_monster_send, y_monster_send, 3);
                }

                if (event.key.keysym.sym == SDLK_DOWN ){

                    x_monster_send=x_monster;
                    y_monster_send=y_monster+1;
                    pacman_sendcharacter_data(sock_fd,ID, x_monster_send, y_monster_send, 3);
                }
            }


            //----------------------------------------------------------------------------------//
            //----------------------------------------------------------------------------------//
            //----------------------------------------------------------------------------------//


            if(event.type == Event_ShownewPlayer){
                Event_ShownewPlayer_Data * data_newplayer = event.user.data1;
                paint_pacman(data_newplayer->xp,data_newplayer->yp,data_newplayer->r,data_newplayer->g,data_newplayer->b);
                paint_monster(data_newplayer->xm, data_newplayer->ym,data_newplayer->r,data_newplayer->g,data_newplayer->b);
                printf("new pacmam x-%d y-%d and new monster x-%d y-%d\n", data_newplayer->xp,data_newplayer->yp,data_newplayer->xm,data_newplayer->ym);
            }
            if(event.type == Event_ShowPacman){
                Event_ShowCharacter_Data * data = event.user.data1;
                clear_place(data->x_old,data->y_old);
                paint_pacman(data->x, data->y,data->r,data->g,data->b);
                printf("move pacman %d x-%d y-%d\n", data->character,data->x,data->y);
            }
            if(event.type == Event_ShowSuperPacman){
                Event_ShowCharacter_Data * data = event.user.data1;
                clear_place(data->x_old,data->y_old);
                paint_powerpacman(data->x, data->y,data->r,data->g,data->b);
                printf("move super pacman x-%d y-%d\n", data->x,data->y);
            }
            if(event.type == Event_ShowMonster) {
                Event_ShowCharacter_Data *data = event.user.data1;
                clear_place(data->x_old, data->y_old);

                if (data->ID==ID) {
                    x_monster = data->x;
                    y_monster = data->y;
                }
                paint_monster(data->x, data->y, data->r, data->g, data->b);
                printf("move monster x-%d y-%d\n", data->x, data->y);

            }
            if(event.type == Event_ShowFruit){
                Event_ShowObject_Data * data_fruit = event.user.data1;
                if (data_fruit->object==4){
                    paint_cherry(data_fruit->x,data_fruit->y);
                }
                else if (data_fruit->object==5){
                    paint_lemon(data_fruit->x,data_fruit->y);
                }else{
                    continue;
                }
            }
            if(event.type == Event_CleanFruit){
                Event_ShowObject_Data * data_fruit = event.user.data1;
                clear_place(data_fruit->x,data_fruit->y);
            }
            if(event.type == Event_Change2Player){
                Event_Change2Player_Data * data_c2c = event.user.data1;

                clear_place(data_c2c->x1_old,data_c2c->y1_old);
                if (data_c2c->character1 == 1) {
                    paint_pacman(data_c2c->x1, data_c2c->y1, data_c2c->r1, data_c2c->g1, data_c2c->b1);
                } else if (data_c2c->character1 == 2) {
                    paint_powerpacman(data_c2c->x1, data_c2c->y1, data_c2c->r1, data_c2c->g1, data_c2c->b1);
                } else if (data_c2c->character1 == 3) {
                    paint_monster(data_c2c->x1, data_c2c->y1, data_c2c->r1, data_c2c->g1, data_c2c->b1);
                    if(data_c2c->ID1==ID) {
                        x_monster = data_c2c->x1;
                        y_monster = data_c2c->y1;

                    }
                }
                if (data_c2c->character2 == 1) {
                    paint_pacman(data_c2c->x2, data_c2c->y2, data_c2c->r2, data_c2c->g2, data_c2c->b2);
                } else if (data_c2c->character2 == 2) {
                    paint_powerpacman(data_c2c->x2, data_c2c->y2, data_c2c->r2, data_c2c->g2, data_c2c->b2);
                } else if (data_c2c->character2 == 3) {
                    paint_monster(data_c2c->x2, data_c2c->y2, data_c2c->r2, data_c2c->g2, data_c2c->b2);

                    if(data_c2c->ID2==ID) {
                        x_monster = data_c2c->x2;
                        y_monster = data_c2c->y2;
                    }
                }
            }

            if(event.type == Event_DeletePlayer){
                Event_DeletePlayer_Data * data_delete = event.user.data1;
                clear_place(data_delete->xp,data_delete->yp);
                clear_place(data_delete->xm,data_delete->ym);
            }




        }
    }
    pacman_disconnect(sock_fd);
    printf("fim\n");
    close_board_windows();
    exit(0);
}
