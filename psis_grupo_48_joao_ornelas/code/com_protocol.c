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


connection_data pacman_connection(char *address, int port_number,int rgb_r,int rgb_g,int rgb_b){
    //----------------------------------------------------//
    // ---------connection to the server socket ----------//
    //----------------------------------------------------//
    connection_data data;
    data.sock_fd=0;
    data.n_col=0;
    data.n_lin=0;
    data.error=0;

    int sock_fd;
    struct sockaddr_in server_addr;
    sock_fd=socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
    if (sock_fd==-1){
        perror("socket: ");
        data.error=-1;
        return (data);
    }
    server_addr.sin_family=AF_INET;
    server_addr.sin_port=htons(port_number);
    if(inet_aton(address, &server_addr.sin_addr)==0){
        printf("The address is not a valid address \n");
        data.error=-1;
        return (data);
    }
    if(connect(sock_fd,(const struct sockaddr *)&server_addr, sizeof(server_addr))==-1){
        perror("Error connecting: ");
        data.error=-1;
        return (data);
    }

    //----------------------------------------------------//
    // --------- sending the colour of the player---------//
    //----------------------------------------------------//
    message m;
    m.action=1;
    m.size= sizeof(colour_msg);

    colour_msg msg_colour;
    msg_colour.r=rgb_r;
    msg_colour.g=rgb_g;
    msg_colour.b=rgb_b;


    send(sock_fd, &m, sizeof(message), 0);
    send(sock_fd, &msg_colour, m.size, 0);

    //return  sock_fd;

    //----------------------------------------------------//
    // -----receiving all the necessary information-------//
    //----------------------------------------------------//

    initialinfo_msg con_msg;
    int err_rcv;
    int i,j;

    err_rcv=recv(sock_fd, &m, sizeof(message),0);
    if (err_rcv<=0){
        perror(" Error in reading: ");
        exit(-1);
    }
    printf("received data size:%d %d\n",err_rcv,m.size);
    err_rcv=recv(sock_fd, &con_msg, m.size,0);
    if (err_rcv<=0){
        perror(" Error in reading: ");
        exit(-1);
    }
    printf("received data size:%d %d\n",err_rcv,m.size);

    err_rcv=recv(sock_fd, &m, sizeof(message),0);
    if (err_rcv<=0){
        perror(" Error in reading: ");
        exit(-1);
    }
    send_board = (int *)malloc(m.size);
    printf("received data size:%d %d\n",err_rcv,m.size);
    err_rcv=recv(sock_fd, send_board, m.size,0);
    if (err_rcv<=0){
        perror(" Error in reading: ");
        exit(-1);
    }
    printf("received data size:%d %d\n",err_rcv,m.size);

    err_rcv=recv(sock_fd, &m, sizeof(message),0);
    if (err_rcv<0){
        perror("1 Error in reading: ");
        exit(-1);
    }
    printf("received data size:%d %d\n",err_rcv,m.size);
    send_list_array = (int *)malloc(m.size);
    err_rcv=recv(sock_fd, send_list_array, m.size,0);
    if (err_rcv<0){
        perror("1 Error in reading: ");
        exit(-1);
    }
    printf("received data size:%d %d\n",err_rcv,m.size);


    data.sock_fd=sock_fd;
    //create board with this player character
    data.n_col=con_msg.n_col;
    data.n_lin=con_msg.n_lin;
    data.xp_ini=con_msg.xpac_ini;
    data.yp_ini=con_msg.ypac_ini;
    data.xm_ini=con_msg.xm_ini;
    data.ym_ini=con_msg.ym_ini;
    data.ID=con_msg.ID;
    create_board_window(con_msg.n_col,con_msg.n_lin);


    paint_pacman(con_msg.xpac_ini, con_msg.ypac_ini,rgb_r,rgb_g,rgb_b);
    paint_monster(con_msg.xm_ini, con_msg.ym_ini,rgb_r,rgb_g,rgb_b);

    printf("%d %d\n", con_msg.n_col, con_msg.n_lin);
    printf("%d %d \n",con_msg.xm_ini,con_msg.ym_ini);
    printf("%d %d \n",con_msg.xpac_ini,con_msg.ypac_ini);

    for ( i = 0 ; i < con_msg.n_lin; i++){
        printf("%2d ", i);
        for ( j = 0 ; j < con_msg.n_col; j++) {
            printf("%d", send_board[i*con_msg.n_col+j]);
        }
        printf("\n");
    }


    int k;
    for ( k = 0 ; k < con_msg.n_entries_of_client_array; k++){
        printf("%2d ", send_list_array[k*4]+10);
    }


    //paint pacman monsters and bricks
    for ( i = 0 ; i < con_msg.n_lin; i++){
        for (j = 0; j < con_msg.n_col; j++){
            if(send_board[i*(con_msg.n_col)+j]==1){
                paint_brick(j, i);
            }else if(send_board[i*(con_msg.n_col)+j]==4){
                paint_cherry(j, i);
            }else if(send_board[i*(con_msg.n_col)+j]==5){
                paint_lemon(j, i);
            }else if(send_board[i*(con_msg.n_col)+j]>10){
                for (k=0;k< con_msg.n_entries_of_client_array;k++){
                    if (send_board[i*(con_msg.n_col)+j]==send_list_array[k*4]+10){
                        paint_pacman(j, i,send_list_array[k*4+1],send_list_array[k*4+2],send_list_array[k*4+3]);
                    }
                }
            }else if (send_board[i*(con_msg.n_col)+j]<-10){
                for (k = 0; k < con_msg.n_entries_of_client_array; k++) {
                    if (send_board[i*(con_msg.n_col)+j] == -(send_list_array[k * 4] + 10)) {
                        paint_monster(j, i,send_list_array[k * 4 + 1], send_list_array[k * 4 + 2], send_list_array[k * 4 + 3]);
                    }
                }
            }
        }
    }
    return data;

}

int pacman_disconnect(int sock_fd){
    int  err_rcv;
    message m;
    m.action=9;
    m.size=0;

    err_rcv=send(sock_fd, &m, sizeof(message), 0);
    if(err_rcv==-1){
        return -1;
    }

    close(sock_fd);
    return 1;

}

int pacman_sendcharacter_data(int sock_fd,int ID, int x, int y, int character){

    int err_recv;
    send_character_pos_msg msg_pos;
    msg_pos.x=x;
    msg_pos.y=y;
    msg_pos.character=character;
    msg_pos.ID=ID;

    message m;
    m.action=2;
    m.size= sizeof(msg_pos);
    err_recv=send(sock_fd, &m, sizeof(message),0);
    err_recv=send(sock_fd, &msg_pos, sizeof(msg_pos),0);
    return err_recv;

}
