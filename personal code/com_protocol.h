typedef struct colour_msg{
    int r;
    int g;
    int b;
} colour_msg;

typedef struct connect_msg{
    int n_lin;
    int n_col;
} connect_msg;

typedef struct character_mov_msg{
    int character; //1-pacman 2-superpacman 3-monster
    int x;
    int y;
    int r;
    int g;
    int b;
} character_mov_msg;



typedef struct object_msg{
//1-cherry 2-lemon 3-brick
    int object;
    int x;
    int y;
}object_msg;
