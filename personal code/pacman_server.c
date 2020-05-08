#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>

//gcc teste.c UI_library.c -o teste-UI -lSDL2 -lSDL2_image

#include "com_protocol.h"
#include "UI_library.h"
#include "list_ID.h"
//#include "board_func.h"

//----------------------------------------------------------//
//Thread responsible for receiving messages from each player//
//----------------and deal with initial protocol------------//
IDList *Clients;
int ** board;

int n_cols;
int n_lines;

struct threadclient_arg{
    int sock_fd,ID;
};
typedef struct pos_struct{
    int x;
    int y;
} pos_struct;

pos_struct get_boardpos(int **board, int char_ID, int n_col,int n_lin){
    int i;
    int j;
    pos_struct pos;
    for(i=0;i<n_lin;i++){
        for (j = 0; j <n_col ; j++){
            if(board[i][j]==char_ID){
                pos.x=j;
                pos.y=i;
                break;
            }
        }
    }
    return pos;


}
void clean_boardpos(int **board,int lin, int col){
    board[lin][col]=0;
    return;


}

Uint32 Event_ShowCharacter;

typedef struct Event_ShowCharacter_Data{
    int character,x,y,x_old,y_old,r,g,b,*sock_fd,ID;
} Event_ShowCharacter_Data;

typedef struct Event_ShowObject_Data{
    int object,x,y,sock_id;
} Event_ShowObject_Data;


void * clientThreadi(void * arg){
    struct threadclient_arg *my_arg = (struct threadclient_arg*)arg;
    int sock_fd=(*my_arg).sock_fd;
    int ID=(*my_arg).ID;
    printf("%d\n",ID);
    //-------------------------------------------------------//
    //-------------------sending board size------------------//
    //-------------------------------------------------------//
    connect_msg con_msg;
    con_msg.n_lin=n_lines;
    con_msg.n_col=n_cols;
    send(sock_fd, &con_msg, sizeof(con_msg), 0);
    printf("sending the size of the board completed\n");


    //-------------------------------------------------------//
    //----------receiving the colour of the character--------//
    //-------------------------------------------------------//
    colour_msg rec_msg;
    int err_rcv;
    err_rcv = recv(sock_fd, &rec_msg , sizeof(rec_msg), 0);
    printf("received %d byte %d %d %d \n", err_rcv, rec_msg.r, rec_msg.g, rec_msg.b);
    if (err_rcv<0){
        perror("Error in reading: ");
        exit(-1);
    }
    //-------------------------------------------------------//
    //-------------------sending initial position------------//
    //-------------------------------------------------------//
    int x_oldp=0;
    int y_oldp=0;
    int x_oldm=0;
    int y_oldm=0;
    board[y_oldp][x_oldp]=10+ID;
    board[y_oldm][x_oldm]=-(10+ID);
    //-------------------------------------------------------//
    int rgb_r=rec_msg.r;
    int rgb_g=rec_msg.g;
    int rgb_b=rec_msg.b;

    //need to make sync here
    Clients=insert_new_ID(Clients,ID, sock_fd,rgb_r,rgb_g,rgb_b);

    //-------------------------------------------------------//
    //---------read the moves made by the client-------------//
    //----loop receiving updated positions  from socket------//
    //-------------------------------------------------------//
    int x_new,y_new,character;
    SDL_Event new_event;
    character_mov_msg msg_pos;
    Event_ShowCharacter_Data *event_data;




    while((err_rcv=recv(sock_fd, &msg_pos, sizeof(msg_pos),0))>0){
        printf("cycle receiving %d byte %d %d %d\n",err_rcv,msg_pos.character,msg_pos.x,msg_pos.y);


        //define the position of the character
        x_new = msg_pos.x;
        y_new = msg_pos.y;
        character=msg_pos.character;

        if(character==1){

            clean_boardpos(board,y_oldp,x_oldp);
            // here see if play is legal
            board[y_new][x_new]=10+ID;
            //

            event_data = malloc(sizeof(Event_ShowCharacter_Data));
            event_data->character=character;
            event_data->x = x_new;
            event_data->y = y_new;
            event_data->x_old = x_oldp;
            event_data->y_old = y_oldp;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;
            event_data->sock_fd=&sock_fd;
            event_data->ID=ID;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowCharacter;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
            x_oldp=x_new;
            y_oldp=y_new;
        }

        if(character==2){
            clean_boardpos(board,x_oldp,y_oldp);
            // here see if play is legal
            board[y_new][x_new]=10+ID;
            //

            event_data = malloc(sizeof(Event_ShowCharacter_Data));
            event_data->character=character;
            event_data->x = x_new;
            event_data->y = y_new;
            event_data->x_old = x_oldp;
            event_data->y_old = y_oldp;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;
            event_data->sock_fd=&sock_fd;
            event_data->ID=ID;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowCharacter;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
            x_oldp=x_new;
            y_oldp=y_new;
        }

        if(character==3){
            clean_boardpos(board,x_oldp,y_oldp);
            // here see if play is legal
            board[y_new][x_new]=10+ID;
            //



            event_data = malloc(sizeof(Event_ShowCharacter_Data));
            event_data->character=character;
            event_data->x = x_new;
            event_data->y = y_new;
            event_data->x_old = x_oldm;
            event_data->y_old = y_oldm;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;
            event_data->sock_fd=&sock_fd;
            event_data->ID=ID;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowCharacter;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
            x_oldm=x_new;
            y_oldm=y_new;

        }

    }
    return(NULL);
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
        ID++;
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

    if(fscanf(fp,"%d %d %*d", &n_cols, &n_lines)!=2) {
        printf("Error reading board  file\n");
        exit(-1);
    }

    create_board_window(n_cols, n_lines);

    char read_object;
    board = malloc(sizeof(int *) * (n_lines+1));
    for ( i = 0 ; i < n_lines+1; i++){
        board[i]=malloc (sizeof(int) * (n_cols+1));
        for (j = 0; j < n_cols+1; j++){
            if(i==n_lines){
                board[i][j]=0;
                continue;
            }

            fscanf(fp,"%c", &read_object);
            if(read_object=='B'){
                paint_brick(j, i);
                board[i][j]=1;
            } else if(read_object=='\n'){
                board[i][j]=0;
            }else board[i][j]=0;

        }
    }
    printf("%d %d\n", n_cols, n_lines);
    for ( i = 0 ; i < n_lines+1; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < n_cols+1; j++) {
            printf("%d", board[i][j]);
        }
        printf("\n");
    }


    fclose(fp);

    //------------------------------------------------//
    // create thread responsible to accept connection //
    //------------------------------------------------//


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






    int rgb_r;
    int rgb_g;
    int rgb_b;
    int x_character;
    int y_character;
    int *sock_fd;

/*------------------------------------------------------------------------//  GAME LOGIC///----------------------------------------------------------*/

    //monster and packman position
    Event_ShowCharacter= SDL_RegisterEvents(1);


    while (!done){
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = SDL_TRUE;
            }
            //------------------------------------------//
            //--------------- Show a PACMAN-------------//
            //------------------------------------------//
            if (event.type == Event_ShowCharacter) {
                character_mov_msg msg;
                // we get the data (created with the malloc)
                Event_ShowCharacter_Data *data = event.user.data1;
                clear_place(data->x_old, data->y_old);


                x_character = data->x;
                y_character = data->y;
                rgb_r = data->r;
                rgb_g = data->g;
                rgb_b = data->b;


                msg.character = data->character;
                msg.x = x_character;
                msg.y = y_character;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;
                sock_fd=data->sock_fd;


                // we paint a pacman
                if(data->character==1) {
                    paint_pacman(x_character, y_character, rgb_r, rgb_g, rgb_b);
                    printf("move pacman x-%d y-%d\n", x_character, y_character);
                    send(*sock_fd, &msg, sizeof(msg), 0);

                    //SYNC MECHANISM
                    IDList *aux = Clients;

                    while(aux != NULL){
                        send(aux->socket, &msg, sizeof(msg), 0);
                        aux = aux->next;

                    }
                }
                // we paint a superpacman
                if(data->character==2) {
                    paint_powerpacman(x_character, y_character, rgb_r, rgb_g, rgb_b);
                    printf("move pacman x-%d y-%d\n", x_character, y_character);

                    //SYNC MECHANISM
                    IDList *aux = Clients;

                    while(aux != NULL){
                        send(aux->socket, &msg, sizeof(msg), 0);
                        aux = aux->next;
                    }

                }
                if(data->character==3) {
                    //we paint a monster
                    paint_monster(x_character, y_character, rgb_r, rgb_g, rgb_b);
                    printf("move pacman x-%d y-%d\n", x_character, y_character);
                    send(*sock_fd, &msg, sizeof(msg), 0);

                    //SYNC MECHANISM
                    IDList *aux = Clients;

                    while(aux != NULL){
                        send(aux->socket, &msg, sizeof(msg), 0);
                        aux = aux->next;
                    }
                }
            }

        }
    }

    printf("fim\n");
    close_board_windows();
    exit(0);
}
