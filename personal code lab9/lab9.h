typedef struct lab9_message_character{
    int x;
    int y;
    int character;
} lab9_message_character;

typedef struct lab9_message_monster{
  int monster_x;
  int monster_y;
  int character;
} lab9_message_monster;

typedef struct lab9_message_pacman{
    int pac_x;
    int pac_y;
    int character;
} lab9_message_pacman;

typedef struct lab9_boardsize{
    int n_lin;
    int n_col;
} lab9_boardsize;

typedef struct lab9_charactercolour{
    int r;
    int g;
    int b;
} lab9_charactercolour;

