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

int sock_fd;
int n_col;
int n_lin;
int rgb_r;
int rgb_g;
int rgb_b;

void * clientThreadi(void * arg){
    //send board sizeS
    lab9_boardsize msg;
    lab9_message_character msg_pos;
    msg.n_lin=n_lin;
    msg.n_col=n_col;
    send(sock_fd, &msg, sizeof(msg), 0);
    printf("just connected to the server\n");

    //receiving the colour of the character
    lab9_charactercolour colour_msg;
    int err_rcv;
    err_rcv = recv(sock_fd, &colour_msg , sizeof(colour_msg), 0);
    printf("received %d byte %d %d %d \n", err_rcv, colour_msg.r, colour_msg.g, colour_msg.b);
    if (err_rcv<0){
        perror("Error in reading: ");
        exit(-1);
    }

    rgb_r=colour_msg.r;
    rgb_g=colour_msg.g;
    rgb_b=colour_msg.b;

    int x,y,character;
    SDL_Event new_event;
    Event_ShowPacman_Data * event_data1;
    Event_ShowMonster_Data * event_data2;
    //read and the moves
    //loop receiving updated positions  from socket
    while((err_rcv=recv(sock_fd, &msg_pos, sizeof(msg_pos),0))>0){
        printf("%d byte %d %d %d\n",err_rcv,msg_pos.character,msg_pos.x,msg_pos.y);


        //define the position of the character
        x = msg_pos.x;
        y = msg_pos.y;
        character=msg_pos.character;

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

int server_socket;

/*///  accept thread  //////*/
void * thread_Accept(void * argc){

    struct sockaddr_in client_addr;
    socklen_t size_addr = sizeof(client_addr);

    pthread_t clientthread_id;
    while(1){
        printf("waiting for connections\n");
        sock_fd= accept(server_socket,(struct sockaddr *) & client_addr, &size_addr);
        if(sock_fd == -1) {
            perror("accept");
            exit(-1);
        }
        pthread_create(&clientthread_id, NULL, clientThreadi, NULL);
        printf("accepted connection \n");
    }
    return (NULL);
}


int main(int argc , char* argv[]){


    if(argc != 3 ){
        exit(-1);
    }
    if(sscanf(argv[1], "%d", &n_lin)!=1){
        printf("argv[1] is not a number\n");
        exit(-1);
    }
    if(sscanf(argv[2], "%d", &n_col)!=1){
        printf("argv[2] is not a number\n");
        exit(-1);
    }
    //creates a windows and a board with 50x20 cases
    create_board_window(n_col, n_lin);

    SDL_Event event;
    int done = 0;
    pthread_t thread_Accept_id;


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
    int err = bind(server_socket, (struct sockaddr *)&server_local_addr,sizeof(server_local_addr));
    if(err == -1) {
        perror("bind");
        exit(-1);
    }
    if(listen(server_socket, 5) == -1) {
        perror("listen");
        exit(-1);
    }

    pthread_create(&thread_Accept_id, NULL, thread_Accept, NULL);



/*------------------------------------------------------------------------//  GAME LOGIC///----------------------------------------------------------*/
    Event_ShowPacman= SDL_RegisterEvents(1);
    Event_ShowMonster= SDL_RegisterEvents(1);

    //monster and packman position


    int x_monster = 0;
    int y_monster = 0;
    int x_pacman = 0;
    int y_pacman = 0;

    while (!done){
        while (SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                done = SDL_TRUE;
            }

            //when the mouse mooves the monster also moves


            if(event.type == Event_ShowPacman){
                lab9_message_pacman msg;
                // we get the data (created with the malloc)
                Event_ShowPacman_Data * data = event.user.data1;
                // retrieve the x and y
                clear_place(x_pacman, y_pacman);
                x_pacman = data->x_p;
                y_pacman = data->y_p;

                msg.character=1;
                msg.pac_x=x_pacman;
                msg.pac_y=y_pacman;

                // we paint a pacman
                paint_pacman(x_pacman, y_pacman,rgb_r,rgb_g,rgb_b);
                printf("move pacman x-%d y-%d\n", x_pacman,y_pacman);
                send(sock_fd, &msg, sizeof(msg),0);


            }
            if(event.type == Event_ShowMonster){
                lab9_message_monster msg;

                // we get the data (created with the malloc)
                Event_ShowMonster_Data * data = event.user.data1;
                // retrieve the x and y
                clear_place(x_monster, y_monster);
                x_monster = data->x_m;
                y_monster = data->y_m;

                msg.character=2;
                msg.monster_x=x_monster;
                msg.monster_y=y_monster;

                // we paint a monster
                paint_monster(x_monster,y_monster,rgb_r,rgb_g,rgb_b);
                printf("move monster x-%d y-%d\n", x_monster,y_monster);
                send(sock_fd, &msg, sizeof(msg),0);


            }

        }
    }

    printf("fim\n");
    close_board_windows();
    exit(0);
}
