#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "lab9.h"
//gcc teste.c UI_library.c -o teste-UI -lSDL2 -lSDL2_image

#include "UI_library.h"
Uint32 Event_ShowPacman;
Uint32 Event_ShowMonster;

//this data will be sent with the event
typedef struct Event_ShowPacman_Data{
    int x_p,y_p,character;
} Event_ShowPacman_Data;

//this data will be sent with the event
typedef struct Event_ShowMonster_Data{
    int x_m, y_m,character;
} Event_ShowMonster_Data;

#include "UI_library.h"

int sock_fd;
void * socketThread(void * arg){
    lab9_message_character msg;
    int err_rcv;

    int x,y,character;
    SDL_Event new_event;
    Event_ShowPacman_Data * event_data1;
    Event_ShowMonster_Data * event_data2;

    printf("just connected to the server\n");


    //loop receiving updated positions  from socket
    while((err_rcv=recv(sock_fd, &msg, sizeof(msg),0))>0){
        printf("receive %d byte %d %d %d\n",err_rcv,msg.character,msg.x,msg.y);


        //define the position of the character
        x = msg.x;
        y = msg.y;
        character=msg.character;

        if(character==1){
        event_data1 = malloc(sizeof(Event_ShowPacman_Data));
        event_data1->x_p = x;
        event_data1->y_p = y;
        event_data1->character=character;

        // clear the event data
        SDL_zero(new_event);
        // define event type
        new_event.type = Event_ShowPacman;
        //assign the event data
        new_event.user.data1 = event_data1;
        // send the event
        SDL_PushEvent(&new_event);
        }

        if(character==2){
            event_data2 = malloc(sizeof(Event_ShowMonster_Data));
            event_data2->x_m = x;
            event_data2->y_m = y;
            event_data2->character=character;

            // clear the event data
            SDL_zero(new_event);
            // define event type
            new_event.type = Event_ShowMonster;
            //assign the event data
            new_event.user.data1 = event_data2;
            // send the event
            SDL_PushEvent(&new_event);
        }


    }
    return(NULL);


}


int main(int argc, char* argv[]){

    if(argc!=6){
        exit(-1);
    }
    int done = 0;
    SDL_Event event;
    Event_ShowPacman= SDL_RegisterEvents(1);
    Event_ShowMonster= SDL_RegisterEvents(1);

    pthread_t socketThread_id;


    // creating socket and connecting to server //
    struct sockaddr_in server_addr;
    sock_fd=socket(AF_INET, SOCK_STREAM,0);
    if (sock_fd==-1){
        perror("socket: ");
        exit(-1);
    }

    server_addr.sin_family=AF_INET;
    int port_number;
    if(sscanf(argv[2], "%d",&port_number)!=1){
        printf("arg[2] is not a number \n");
        exit(-1);
    }
    server_addr.sin_port=htons(port_number);

    if(inet_aton(argv[1], &server_addr.sin_addr)==0){
        printf("argv[1] is not a valid address \n");
        exit(-1);
    }
    printf("connecting to %s %d\n", argv[1],server_addr.sin_port);


    if(connect(sock_fd,(const struct sockaddr *)&server_addr, sizeof(server_addr))==-1){
        perror("Error connecting: ");
        exit(-1);
    }

    lab9_boardsize boardsize_msg;
    int err_rcv;
    int n_col;
    int n_lin;

    err_rcv=recv(sock_fd, &boardsize_msg, sizeof(boardsize_msg),0);
    if (err_rcv<0){
        perror("Error in reading: ");
        exit(-1);
    }

    printf("%d byte %d %d \n",err_rcv,boardsize_msg.n_lin,boardsize_msg.n_col);
    //create board
    n_col=boardsize_msg.n_col;
    n_lin=boardsize_msg.n_lin;
    create_board_window(n_col,n_lin);


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


    lab9_charactercolour colour_msg;
    colour_msg.r=rgb_r;
    colour_msg.g=rgb_g;
    colour_msg.b=rgb_b;
    send(sock_fd, &colour_msg, sizeof(colour_msg), 0);



    // create thread to receive messages//
    pthread_create(&socketThread_id, NULL, socketThread,NULL);



    //monster and packman position
    int x_pacman = 0;
    int y_pacman = 0;
    int x_monster = 0;
    int y_monster = 0;
    int x_pacman_rec = 0;
    int y_pacman_rec = 0;
    int x_monster_rec = 0;
    int y_monster_rec = 0;


    while (!done){
        while (SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                done = SDL_TRUE;
            }

            //when the mouse mooves the monster also moves
            if(event.type == SDL_MOUSEMOTION){

                //this fucntion return the place cwher the mouse cursor is
                get_board_place(event.motion.x, event.motion.y,&x_pacman, &y_pacman);
                lab9_message_pacman msg;

                printf("send pacman position x-%d y-%d\n", x_pacman,y_pacman);
                msg.pac_x=x_pacman;
                msg.pac_y=y_pacman;
                msg.character=1;
                send(sock_fd, &msg, sizeof(msg),0);
            }


            if(event.type == Event_ShowPacman){
               // we get the data (created with the malloc)
                Event_ShowPacman_Data * data = event.user.data1;
                // retrieve the x and y
                clear_place(x_pacman_rec, y_pacman_rec);
                x_pacman_rec = data->x_p;
                y_pacman_rec = data->y_p;

                // we paint a pacman
                paint_pacman(x_pacman_rec, y_pacman_rec,rgb_r,rgb_g,rgb_b);
                printf("move pacman x-%d y-%d\n", x_pacman_rec,y_pacman_rec);


            }
            if(event.type == Event_ShowMonster){
                // we get the data (created with the malloc)
                Event_ShowMonster_Data * data = event.user.data1;
                // retrieve the x and y
                clear_place(x_monster_rec, y_monster_rec);
                x_monster_rec = data->x_m;
                y_monster_rec = data->y_m;

                // we paint a monster
                paint_monster(x_monster_rec,y_monster_rec,rgb_r,rgb_g,rgb_b);
                printf("move pacman x-%d y-%d\n", x_monster_rec,y_monster_rec);

            }
            if(event.type == SDL_KEYDOWN) {



                if (event.key.keysym.sym == SDLK_RIGHT ){
                    lab9_message_monster msg;

                    x_monster=x_monster+1;

                    printf("send  position x-%d y-%d\n", x_monster,y_monster);
                    msg.monster_x=x_monster;
                    msg.monster_y=y_monster;
                    msg.character=2;
                    send(sock_fd, &msg, sizeof(msg),0);
                }
                if (event.key.keysym.sym == SDLK_LEFT ){
                    lab9_message_monster msg;

                    x_monster=x_monster-1;

                    printf("send  position x-%d y-%d\n", x_monster,y_monster);
                    msg.monster_x=x_monster;
                    msg.monster_y=y_monster;
                    msg.character=2;
                    send(sock_fd, &msg, sizeof(msg),0);
                }
                if (event.key.keysym.sym == SDLK_UP ){
                    lab9_message_monster msg;

                    y_monster=y_monster-1;

                    printf("send  position x-%d y-%d\n", x_monster,y_monster);
                    msg.monster_x=x_monster;
                    msg.monster_y=y_monster;
                    msg.character=2;
                    send(sock_fd, &msg, sizeof(msg),0);
                }

                if (event.key.keysym.sym == SDLK_DOWN ){
                    lab9_message_monster msg;

                    y_monster=y_monster+1;

                    printf("send  position x-%d y-%d\n", x_monster,y_monster);
                    msg.monster_x=x_monster;
                    msg.monster_y=y_monster;
                    msg.character=2;
                    send(sock_fd, &msg, sizeof(msg),0);

                }
            }

        }
    }

    printf("fim\n");
    close_board_windows();
    exit(0);
}
