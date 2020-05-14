#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#include <semaphore.h>

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

typedef struct threadclient_arg{
    int sock_fd,ID;
}threadclient_arg;

typedef struct sem_nplayspersec{
    sem_t sem_nplay1,sem_nplay2;
}sem_nplayspersec;

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

int set_initialpos(int **board, int n_col,int n_lin){
    int i;
    int j;
    int emptyspaces=0;
    int random_entry;
    for(i=0;i<n_lin;i++){
        for (j = 0; j <n_col ; j++){
            if(board[i][j]==0){
                emptyspaces++;
            }
        }
    }
    if(emptyspaces<2){
        return -1;
    }
    random_entry = (rand()%(emptyspaces-1));
    printf("random entry: %d\n",random_entry);
    printf("number of empty spaces: %d\n",emptyspaces);
    emptyspaces=0;
    for(i=0;i<n_lin;i++){
        for (j = 0; j <n_col ; j++){
            if(board[i][j]==0){
                if(random_entry==emptyspaces){
                    return i*n_col+j;
                }
                emptyspaces++;
            }
        }
    }
    return -1;
}


Uint32 Event_ShowCharacter;
Uint32 Event_Change2Characters;
Uint32 Event_ShownewCharacter;

typedef struct Event_ShowCharacter_Data{
    int character,x,y,x_old,y_old,r,g,b;
} Event_ShowCharacter_Data;

typedef struct Event_Change2Characters_Data{
    int character1,x1,y1,character2,x2,y2,r1,g1,b1,r2,g2,b2;
} Event_Change2Characters_Data;

typedef struct Event_ShownewCharacter_Data{
    int xp,yp,xm,ym,r,g,b,sock_fd;
} Event_ShownewCharacter_Data;

typedef struct Event_ShowObject_Data{
    int object,x,y,sock_id;
} Event_ShowObject_Data;


void * thr_forplayspersec(void * arg) {

    //struct sem_nplayspersec *my_arg = (struct sem_nplayspersec*)arg;
    //while(1){
      //  sem_wait(my_arg->sem_nplay1);
       // usleep(500000);
       // sem_post(my_arg->sem_nplay2);
    //}
    while (1);
}

void * clientThreadi(void * arg){
    struct threadclient_arg *my_arg = (struct threadclient_arg*)arg;
    int sock_fd=(*my_arg).sock_fd;
    int ID=(*my_arg).ID;
    int ID_other;
    //-------------------------------------------------------//
    //----sending board size and sending initial position----//
    //-------------------------------------------------------//

    connect_msg con_msg;
    con_msg.n_lin=n_lines;
    con_msg.n_col=n_cols;



    //sync needed dont use board directly
    int elem=set_initialpos(board, n_cols,n_lines);
    if(elem<0){
        printf("board is full \n");
        //close thread and etc
    }
    int x_oldp=elem%n_cols;
    int y_oldp=elem/n_cols;

    elem=set_initialpos(board, n_cols,n_lines);
    if(elem<0){
        printf("board is full \n");
        //close thread and etc
    }

    int x_oldm=elem%n_cols;
    int y_oldm=elem/n_cols;


    board[y_oldp][x_oldp]=10+ID;
    printf("initial pos: x %d,y%d \n",x_oldp,y_oldp);
    board[y_oldm][x_oldm]=-(10+ID);
    con_msg.xpac_ini=x_oldp;
    con_msg.ypac_ini=y_oldp;
    con_msg.xm_ini=x_oldm;
    con_msg.ym_ini=y_oldm;


    send(sock_fd, &con_msg, sizeof(con_msg), 0);
    printf("sending initial informations completed\n");

    int i,j;

    int * ini_board = (int *)malloc(sizeof(int)* (n_cols+1)*(n_lines+1));
    for(i=0;i<n_lines+1;i++) {
        for (j = 0; j < n_cols + 1; j++) {
        ini_board[i*(n_cols+1)+j]=board[i][j];
        //printf("%d\n",(i*n_cols)+j);
        }
    }
    for ( i = 0 ; i < n_lines+1; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < n_cols+1; j++) {
            printf("%d", ini_board[i*(n_cols+1)+j]);
        }
        printf("\n");
    }

    int err=send(sock_fd, ini_board, ((n_lines+1)*(n_cols+1))*sizeof(int), 0);
    printf("%d\n",err);


    IDList * send_list  = (IDList *) malloc(sizeof(IDList));
    send_list=Clients;
    int size=0;
    while(send_list!=NULL){
        size++;
        send_list=get_next_ID(send_list);
    }
    int * send_list_array = (int *)malloc(sizeof(int)*size*4);
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
    for(i=0;i<size;i++){
        printf("%d",send_list_array[i]);
        printf("%d",send_list_array[i+1]);
        printf("%d",send_list_array[i+2]);
        printf("%d\n",send_list_array[i+3]);
    }
    err=send(sock_fd, &size, sizeof(size), 0);
    printf(",,, %d\n",err);
    err=send(sock_fd, send_list_array, sizeof(int)*size*4, 0);
    printf("ola %d\n",err);








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
    int rgb_r=rec_msg.r;
    int rgb_g=rec_msg.g;
    int rgb_b=rec_msg.b;
    int rgb_r_other;
    int rgb_g_other;
    int rgb_b_other;


    //-------------------------------------------------------//
    //paint and send initial positions to all other clients--//
    //-------------------------------------------------------//
    SDL_Event new_event;
    character_mov_msg msg_pos;
    Event_ShownewCharacter_Data *event_data_ini;

    event_data_ini = malloc(sizeof(Event_ShownewCharacter_Data));
    event_data_ini->xp = x_oldp;
    event_data_ini->yp = y_oldp;
    event_data_ini->xm = x_oldm;
    event_data_ini->ym= y_oldm;
    event_data_ini->r = rgb_r;
    event_data_ini->g = rgb_g;
    event_data_ini->b = rgb_b;

    SDL_zero(new_event);
    new_event.type = Event_ShownewCharacter;
    new_event.user.data1 = event_data_ini;
    SDL_PushEvent(&new_event);



    //-------------------------------------------------------//
    //----------------Insert new client info-----------------//
    //-------------------------------------------------------//
    IDList * this_client = (IDList *) malloc(sizeof(IDList));
    IDList * other_list= (IDList *) malloc(sizeof(IDList));
    if(this_client == NULL){
        return NULL;
    }

    this_client->ID = ID;
    this_client->socket=sock_fd;
    this_client->rgb_r = rgb_r;
    this_client->rgb_g = rgb_g;
    this_client->rgb_b = rgb_b;
    this_client->next = NULL;
    //need to make sync here
    Clients=insert_new_ID(Clients, this_client);
    //-------------------------------------------------------//
    //---------read the moves made by the client-------------//
    //----loop receiving updated positions  from socket------//
    //-------------------------------------------------------//
    int x_new,y_new,x2,y2,character,character2,flag_2player=0;
    Event_ShowCharacter_Data *event_data;
    event_data = malloc(sizeof(Event_ShowCharacter_Data));

    Event_Change2Characters_Data *event_data2player;
    event_data2player = malloc(sizeof(Event_Change2Characters_Data));

    //-------------------------------------------------------//
    //---------initiliaze sync variables for-------- --------//
    //-------------------------------------------------------//
    printf("ola \n");
    /*
    struct sem_nplayspersec arg_sem;
    int error;
    arg_sem = malloc(sizeof(struct sem_nplayspersec));
    int sem_value;
    pthread_t thr_nplayspersec;
    printf("ola \n");
    sem_init(arg_sem->sem_nplay1, 0, 0);
    sem_init(arg_sem->sem_nplay2, 0, 1);
    printf("ola\n");
    error = pthread_create(&thr_nplayspersec,NULL,thr_forplayspersec , &arg_nplaypersec);
    printf("ola\n");
     */



    while((err_rcv=recv(sock_fd, &msg_pos, sizeof(msg_pos),0))>0){
        printf("cycle receiving %d byte %d %d %d\n",err_rcv,msg_pos.character,msg_pos.x,msg_pos.y);
        x_new = msg_pos.x;
        y_new = msg_pos.y;
        character=msg_pos.character;

        //sem_getvalue(arg_sem->sem_nplay2, &sem_value);
        //if(sem_value==0){
        //    continue;
        //}

        if(character==-1){
            //clean this client of client list
            Clients =remove_ID(Clients,this_client);
            //clean this client of the board representation
            board[y_oldp][x_oldp]=0;
            board[y_oldm][x_oldm]=0;


            event_data->character=-1;
            event_data->x = x_oldm;
            event_data->y = y_oldm;
            event_data->x_old = x_oldp;
            event_data->y_old = y_oldp;
            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowCharacter;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);

            //clear socket
            puts("client disconnected");
            close(sock_fd);
            pthread_exit(NULL);
        }

        if(character==1){
            //check if position has changed
            if(x_new==x_oldp && y_new==y_oldp){
                continue;
            }

            for ( i = 0 ; i < n_lines+1; i++){
                printf("%2d ", i);
                for ( j = 0 ; j < n_cols+1; j++) {
                    printf("%d", board[i][j]);
                }
                printf("\n");
            }
            // here see if play is legal
            // check if it is the 4 adjacent places (down, up,left,right)
            board[y_oldp][x_oldp]=0;

            if(((x_new-1==x_oldp || x_new+1==x_oldp)&& y_new==y_oldp)||((y_new-1==y_oldp || y_new+1==y_oldp)&& x_new==x_oldp)){
                //valid
                if(board[y_new][x_new]>10){
                    ID_other=board[y_new][x_new]-10;
                    board[y_oldp][x_oldp]=board[y_new][x_new];
                    board[y_new][x_new]=10+ID;
                    flag_2player=1;
                    character2=1;
                    x2=x_oldp;
                    y2=y_oldp;
                    other_list=get_IDlist(Clients,ID_other);
                    printf("ola\n");
                    rgb_r_other=other_list->rgb_r;
                    rgb_g_other=other_list->rgb_g;
                    rgb_b_other=other_list->rgb_b;
                }
                else if(board[y_new][x_new]==-(ID+10)){
                    board[y_oldp][x_oldp]=board[y_new][x_new];
                    board[y_new][x_new]=ID+10;
                    flag_2player=1;
                    character2=3;
                    x2=x_oldp;
                    y2=y_oldp;
                    rgb_r_other=rgb_r;
                    rgb_g_other=rgb_g;
                    rgb_b_other=rgb_b;

                }
                else if(x_new!=x_oldp){
                    if(x_new>(n_cols-1)){
                        if(board[y_new][n_cols-2]== 0 ){
                            board[y_new][n_cols-2]=10+ID;
                            x_new= n_cols-2;
                        }
                        else{
                            x_new=x_oldp;
                        }
                    }
                    else if(x_new<0){
                        if(board[y_new][1]== 0 ){
                            board[y_new][1]=10+ID;
                            x_new=1;
                        }
                        else{
                            x_new=x_oldp;
                        }
                    }
                    else if(x_new>x_oldp){
                        if(board[y_new][x_new]==1){
                            if(board[y_new][x_new-2]!=1 && x_new-2>=0) {
                                x_new = x_new - 2;
                            }
                            else {
                                x_new=x_oldp;
                            }
                        }
                    }
                    else if(x_new<x_oldp){
                        if(board[y_new][x_new]==1){
                            if(board[y_new][x_new+2]!=1 && x_new+2<=n_cols-1) {
                                x_new = x_new + 2;
                            }
                             else {
                                x_new = x_oldp;
                            }
                        }
                    }
                }
                else if(y_new!=y_oldp){
                    if(y_new>(n_lines-1)){
                        if(board[n_lines-2][x_new]== 0 ){
                            board[n_lines-2][x_new]=10+ID;
                            y_new= n_lines-2;
                        }
                        else{
                            y_new=y_oldp;
                        }
                    }
                    if(y_new<0){
                        if(board[1][x_new]== 0 ){
                            board[1][x_new]=10+ID;
                            y_new= 1;
                        }
                        else{
                            y_new=y_oldp;
                        }
                    }
                    if(y_new>y_oldp){
                        if(board[y_new][x_new]==1){
                            if(board[y_new-2][x_new]!=1 && y_new-2>=0) {
                                y_new = y_new - 2;
                            }
                            else {
                                y_new=y_oldp;
                            }
                        }
                    }
                    if(y_new<y_oldp){
                        if(board[y_new][x_new]==1){
                            if( board[y_new+2][x_new]!=1 && y_new+2<=n_lines-1) {
                                y_new = y_new + 2;
                            }
                            else {
                                y_new = y_oldp;
                            }
                        }
                    }
                }



            }
            else{
                continue;
            }
            // check if it going against a brick or the limit of the board it will be bounced back
            //if the character cannot be bounced back it will stay at the same place

            board[y_new][x_new]=10+ID;

            if (flag_2player==0) {
                event_data->character = character;
                event_data->x = x_new;
                event_data->y = y_new;
                event_data->x_old = x_oldp;
                event_data->y_old = y_oldp;
                event_data->r = rgb_r;
                event_data->g = rgb_g;
                event_data->b = rgb_b;

                // send the event
                SDL_zero(new_event);
                new_event.type = Event_ShowCharacter;
                new_event.user.data1 = event_data;
                SDL_PushEvent(&new_event);
                x_oldp = x_new;
                y_oldp = y_new;
            }
            if (flag_2player==1){
                event_data2player->character1 = character;
                event_data2player->x1 = x_new;
                event_data2player->y1 = y_new;
                event_data2player->character2 = character2;
                event_data2player->x2 = x2;
                event_data2player->y2 = y2;
                event_data2player->r1 = rgb_r;
                event_data2player->g1 = rgb_g;
                event_data2player->b1 = rgb_b;
                event_data2player->r2 = rgb_r_other;
                event_data2player->g2 = rgb_g_other;
                event_data2player->b2 = rgb_b_other;

                // send the event
                SDL_zero(new_event);
                new_event.type = Event_Change2Characters;
                new_event.user.data1 = event_data2player;
                SDL_PushEvent(&new_event);
                x_oldp = x_new;
                y_oldp = y_new;

                flag_2player=0;

            }
        }

        if(character==2){
            //check if position has changed
            if(x_new==x_oldp && y_new==y_oldp){
                continue;
            }

            board[y_oldp][x_oldp]=0;
            // here see if play is legal
            board[y_new][x_new]=10+ID;
            //
            event_data->character=character;
            event_data->x = x_new;
            event_data->y = y_new;
            event_data->x_old = x_oldp;
            event_data->y_old = y_oldp;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowCharacter;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
            x_oldp=x_new;
            y_oldp=y_new;
        }

        if(character==3){
            if(x_new==x_oldm && y_new==y_oldm){
                continue;
            }


            // here see if play is legal
            // check if it is the 4 adjacent places (down, up,left,right)
            if(((x_new-1==x_oldm || x_new+1==x_oldm)&& y_new==y_oldm)||((y_new-1==y_oldm || y_new+1==y_oldm)&& x_new==x_oldm)){
                //valid
                board[y_oldm][x_oldm]=0;
            }
            else{
                continue;
            }
            // check if it going against a brick or the limit of the board it will be bounced back
            //if the character cannot be bounced back it will stay at the same place

            board[y_new][x_new]=-(10+ID);

            event_data->character=character;
            event_data->x = x_new;
            event_data->y = y_new;
            event_data->x_old = x_oldm;
            event_data->y_old = y_oldm;
            event_data->r = rgb_r;
            event_data->g = rgb_g;
            event_data->b = rgb_b;

            // send the event
            SDL_zero(new_event);
            new_event.type = Event_ShowCharacter;
            new_event.user.data1 = event_data;
            SDL_PushEvent(&new_event);
            x_oldm=x_new;
            y_oldm=y_new;

        }
        /*
        sem_wait(arg_sem->sem_nplay2);
        sem_post(arg_sem->sem_nplay1);
        */

    }

    Clients =remove_ID(Clients,this_client);

    //clean this client of the board representation
    board[y_oldp][x_oldp]=0;
    board[y_oldm][x_oldm]=0;


    event_data = malloc(sizeof(Event_ShowCharacter_Data));
    event_data->character=-1;
    event_data->x = x_oldm;
    event_data->y = y_oldm;
    event_data->x_old = x_oldp;
    event_data->y_old = y_oldp;
    event_data->r = 0;
    event_data->g = 0;
    event_data->b = 0;

    // send the event
    SDL_zero(new_event);
    new_event.type = Event_ShowCharacter;
    new_event.user.data1 = event_data;
    SDL_PushEvent(&new_event);


    //clear socket
    puts("client disconnected");
    close(sock_fd);
    printf("%d %d\n", n_cols, n_lines);
    for ( i = 0 ; i < n_lines+1; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < n_cols+1; j++) {
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
    board =(int **) malloc(sizeof(int *) * (n_lines+1));
    for ( i = 0 ; i < n_lines+1; i++){
        board[i]=(int *)malloc(sizeof(int) * (n_cols+1));
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
    int x_newp;
    int y_newp;
    int x_newm;
    int y_newm;

/*------------------------------------------------------------------------//  GAME LOGIC///----------------------------------------------------------*/

    //monster and packman position
    Event_ShowCharacter= SDL_RegisterEvents(1);
    Event_Change2Characters= SDL_RegisterEvents(1);
    Event_ShownewCharacter= SDL_RegisterEvents(1);



    while (!done){
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = SDL_TRUE;
            }
            //------------------------------------------//
            //--- Show a new PACMAN and new Monster-----//
            //------------------------------------------//
            if (event.type == Event_ShownewCharacter) {
                character_mov_msg msg;
                // we get the data (created with the malloc)
                Event_ShownewCharacter_Data *data = event.user.data1;

                x_newp = data->xp;
                y_newp = data->yp;
                x_newm = data->xm;
                y_newm = data->ym;
                rgb_r = data->r;
                rgb_g = data->g;
                rgb_b = data->b;

                paint_pacman(x_newp, y_newp, rgb_r, rgb_g, rgb_b);
                printf("new pacman x-%d y-%d\n", x_newp, y_character);
                paint_monster(x_newm, y_newm, rgb_r, rgb_g, rgb_b);
                printf("new monster x-%d y-%d\n", x_newm, y_newm);

                msg.character = 4;
                msg.x = x_newp;
                msg.y = y_newp;
                msg.x_old = x_newm;
                msg.y_old = y_newm;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;
                //SYNC MECHANISM
                IDList *aux = Clients;
                while(aux != NULL){
                    send(aux->socket, &msg, sizeof(msg), 0);
                    aux = aux->next;

                }
            }
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
                printf("%d\n",data->character);
                msg.x = x_character;
                msg.y = y_character;
                msg.x_old = data->x_old;
                msg.y_old = data->y_old;
                msg.r = data->r;
                msg.g = data->g;
                msg.b = data->b;


                // we paint a pacman
                if(data->character==1) {
                    paint_pacman(x_character, y_character, rgb_r, rgb_g, rgb_b);
                    printf("move pacman x-%d y-%d\n", x_character, y_character);

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
                    printf("move monster x-%d y-%d\n", x_character, y_character);

                    //SYNC MECHANISM
                    IDList *aux = Clients;

                    while(aux != NULL){
                        send(aux->socket, &msg, sizeof(msg), 0);
                        aux = aux->next;
                    }
                }
                if(data->character==-1) {
                    clear_place(x_character,y_character);

                    printf("clear -%d y-%d\n", x_character, y_character);
                    printf("clear -%d y-%d\n", data->x_old, data->y_old);

                    //SYNC MECHANISM
                    IDList *aux = Clients;

                    while(aux != NULL){
                        printf("send+\n");
                        send(aux->socket, &msg, sizeof(msg), 0);
                        aux = aux->next;

                    }
                }
            }

            //*********************************//
            //*********************************//
            //*********************************//
            if (event.type == Event_Change2Characters) {

                // we get the data (created with the malloc)
                Event_Change2Characters_Data *data = event.user.data1;
                if(data->character1==1){
                    paint_pacman(data->x1,data->y1,data->r1,data->g1,data->b1);
                }
                else if(data->character1==2){
                    paint_powerpacman(data->x1,data->y1,data->r1,data->g1,data->b1);
                }
                else if(data->character1==3){
                    paint_monster(data->x1,data->y1,data->r1,data->g1,data->b1);
                }
                printf("ola %d\n",data->character2);
                if(data->character2==1){
                    paint_pacman(data->x2,data->y2,data->r2,data->g2,data->b2);
                }
                else if(data->character2==2){
                    paint_powerpacman(data->x2,data->y2,data->r2,data->g2,data->b2);
                }
                else if(data->character2==3){
                    paint_monster(data->x2,data->y2,data->r2,data->g2,data->b2);
                }
            }

        }
    }

    printf("fim\n");
    close_board_windows();
    exit(0);
}
