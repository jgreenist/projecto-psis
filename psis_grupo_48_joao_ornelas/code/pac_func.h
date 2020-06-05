typedef struct threadclient_arg{
    int sock_fd,ID;
}threadclient_arg;

typedef struct sem_nplayspersec{
    sem_t sem_nplay1,sem_nplay2,sem_end;
}sem_nplayspersec;

typedef struct sem_inactivity{
    sem_t sem_inactivity;
    sem_t sem_end;
    int ID,rgb_r,rgb_g,rgb_b,character;
}sem_inactivity;

typedef struct pos_struct{
    int x;
    int y;
} pos_struct;

typedef struct fruit_alloc{
    sem_t sem_fruit1;
    sem_t sem_fruit2;
    int fruit;
}fruit_alloc;

int set_initialpos(int **board, int n_col,int n_lin);

pos_struct  get_boardpos(int **board, int char_ID, int n_col,int n_lin);

