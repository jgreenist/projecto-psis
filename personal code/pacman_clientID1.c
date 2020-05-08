#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

//gcc teste.c UI_library.c -o teste-UI -lSDL2 -lSDL2_image
#include "com_protocol.h"
#include "UI_library.h"


Uint32 Event_ShowPacman;
Uint32 Event_ShowSuperPacman;
Uint32 Event_ShowMonster;

typedef struct Event_ShowCharacter_Data{
    int character,x,y,r,g,b;
} Event_ShowCharacter_Data;

typedef struct Event_ShowObject_Data{
    int object,x,y;
} Event_ShowObject_Data;


int sock_fd;
void * socketThread(void * arg){

    int x,y,character,rgb_r,rgb_g,rgb_b;
    SDL_Event new_event;
    Event_ShowCharacter_Data *event_data;
    character_mov_msg msg_pos;
    int err_rcv;

    printf("just connected to the server\n");


    //loop receiving updated positions  from socket
    while((err_rcv=recv(sock_fd, &msg_pos, sizeof(msg_pos),0))>0){
        printf("receive %d byte %d %d %d\n",err_rcv,msg_pos.character,msg_pos.x,msg_pos.y);

        //define the position of the character
        x = msg_pos.x;
        y = msg_pos.y;
        character=msg_pos.character;
        rgb_r=msg_pos.r;
        rgb_g=msg_pos.g;
        rgb_b=msg_pos.b;

        if(character==1){
            event_data = malloc(sizeof(Event_ShowCharacter_Data));
            event_data->character=character;
            event_data->x = x;
            event_data->y = y;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;


            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowPacman;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
        }

        if(character==2){
            event_data = malloc(sizeof(Event_ShowCharacter_Data));
            event_data->character=character;
            event_data->x = x;
            event_data->y = y;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;

            //  send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowSuperPacman;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
        }

        if(character==3){
            event_data = malloc(sizeof(Event_ShowCharacter_Data));
            event_data->character=character;
            event_data->x = x;
            event_data->y = y;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowMonster;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
        }
    }
    return(NULL);


}


int main(int argc, char* argv[]){

    if(argc!=6){
        exit(-1);
    }
    SDL_Event event;
    int done = 0;

    Event_ShowPacman= SDL_RegisterEvents(1);
    Event_ShowSuperPacman= SDL_RegisterEvents(1);
    Event_ShowMonster= SDL_RegisterEvents(1);


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

    connect_msg con_msg;
    int err_rcv;
    int n_col;
    int n_lin;

    err_rcv=recv(sock_fd, &con_msg, sizeof(con_msg),0);
    if (err_rcv<0){
        perror("Error in reading: ");
        exit(-1);
    }

    printf("%d byte %d %d \n",err_rcv,con_msg.n_lin,con_msg.n_col);
    //create board
    n_col=con_msg.n_col;
    n_lin=con_msg.n_lin;
    create_board_window(n_col,n_lin);


    colour_msg send_msg;
    send_msg.r=rgb_r;
    send_msg.g=rgb_g;
    send_msg.b=rgb_b;
    send(sock_fd, &send_msg, sizeof(send_msg), 0);


    pthread_t socketThread_id;
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
                character_mov_msg msg_pos;

                printf("send pacman position x-%d y-%d\n", x_pacman,y_pacman);
                msg_pos.x=x_pacman;
                msg_pos.y=y_pacman;
                msg_pos.character=1;
                send(sock_fd, &msg_pos, sizeof(msg_pos),0);
            }


            if(event.type == Event_ShowPacman){
                // we get the data (created with the malloc)
                Event_ShowCharacter_Data * data = event.user.data1;
                // retrieve the x and y
                clear_place(x_pacman_rec, y_pacman_rec);
                x_pacman_rec = data->x;
                y_pacman_rec = data->y;
                rgb_r=data->r;
                rgb_g=data->g;
                rgb_b=data->b;


                // we paint a pacman
                paint_pacman(x_pacman_rec, y_pacman_rec,rgb_r,rgb_g,rgb_b);
                printf("move pacman x-%d y-%d\n", x_pacman_rec,y_pacman_rec);


            }
            if(event.type == Event_ShowMonster){
                // we get the data (created with the malloc)
                Event_ShowCharacter_Data * data = event.user.data1;
                // retrieve the x and y
                clear_place(x_monster_rec, y_monster_rec);
                x_monster_rec = data->x;
                y_monster_rec = data->y;
                rgb_r=data->r;
                rgb_g=data->g;
                rgb_b=data->b;

                // we paint a monster
                paint_monster(x_monster_rec,y_monster_rec,rgb_r,rgb_g,rgb_b);
                printf("move monster x-%d y-%d\n", x_monster_rec,y_monster_rec);
            }

            if(event.type == SDL_KEYDOWN) {



                if (event.key.keysym.sym == SDLK_RIGHT ){
                    character_mov_msg msg;

                    x_monster=x_monster+1;

                    printf("send monster position x-%d y-%d\n", x_monster,y_monster);
                    msg.x=x_monster;
                    msg.y=y_monster;
                    msg.character=3;
                    send(sock_fd, &msg, sizeof(msg),0);
                }
                if (event.key.keysym.sym == SDLK_LEFT ){
                    character_mov_msg msg;

                    x_monster=x_monster-1;

                    printf("send monster  position x-%d y-%d\n", x_monster,y_monster);
                    msg.x=x_monster;
                    msg.y=y_monster;
                    msg.character=3;
                    send(sock_fd, &msg, sizeof(msg),0);
                }
                if (event.key.keysym.sym == SDLK_UP ){
                    character_mov_msg msg;

                    y_monster=y_monster-1;

                    printf("send monster position x-%d y-%d\n", x_monster,y_monster);
                    msg.x=x_monster;
                    msg.y=y_monster;
                    msg.character=3;
                    send(sock_fd, &msg, sizeof(msg),0);
                }

                if (event.key.keysym.sym == SDLK_DOWN ){
                    character_mov_msg msg;

                    y_monster=y_monster+1;

                    printf("send monster position x-%d y-%d\n", x_monster,y_monster);
                    msg.x=x_monster;
                    msg.y=y_monster;
                    msg.character=3;
                    send(sock_fd, &msg, sizeof(msg),0);

                }
            }

        }
    }

    printf("fim\n");
    close_board_windows();
    exit(0);
}
