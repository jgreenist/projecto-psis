typedef struct msg_struct{
    int action;
    int size;
} message;


typedef struct colour_msg{
    int r;
    int g;
    int b;
} colour_msg;

typedef struct initialinfo_msg{
    int ID;
    int n_lin;
    int n_col;
    int xpac_ini;
    int ypac_ini;
    int xm_ini;
    int ym_ini;
    int n_entries_of_client_array;
} initialinfo_msg;

int * send_board;
int * send_list_array;
int * score_msg;

//rec is viewed as the client side(server to client)
//send is also viewed as the client side (client to server)

typedef struct connection_data{
    int sock_fd,ID,n_col,n_lin,xp_ini,yp_ini,xm_ini,ym_ini,error;
} connection_data;

typedef struct send_character_pos_msg{
    int ID;
    int character; //1-pacman 2-superpacman 3-monster
    int x;
    int y;
} send_character_pos_msg;

typedef struct rec_character_pos_msg{
    int ID;
    int character; //1-pacman 2-superpacman 3-monster
    int x1;
    int y1;
    int x2;
    int y2;
    int r;
    int g;
    int b;
} rec_character_pos_msg;

typedef struct rec_change2characters_msg{
    int character1; //1-pacman 2-superpacman 3-monster
    int x1_old;
    int y1_old;
    int x1;
    int y1;
    int r1;
    int g1;
    int b1;
    int ID1;
    int character2;
    int x2;
    int y2;
    int r2;
    int g2;
    int b2;
    int ID2;
} rec_change2characters_msg;


typedef struct rec_object_msg{
//1-cherry 2-lemon
    int object;
    int x;
    int y;
}rec_object_msg;


connection_data pacman_connection(char *address, int port_number,int rgb_r,int rgb_g,int rgb_b);
int pacman_disconnect(int sock_fd);
int pacman_sendcharacter_data(int sock_fd,int ID, int x, int y, int character);