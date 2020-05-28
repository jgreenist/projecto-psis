#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include "pac_func.h"


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
    if(emptyspaces==1){
        for(i=0;i<n_lin;i++){
            for (j = 0; j <n_col ; j++){
                if(board[i][j]==0) {
                    return i * n_col + j;
                }
            }
        }
    }else if(emptyspaces==0){
        return -1;
    } else {
        random_entry = (rand() % (emptyspaces - 1));
        printf("random entry: %d\n", random_entry);
        printf("number of empty spaces: %d\n", emptyspaces);
        emptyspaces = 0;
        for (i = 0; i < n_lin; i++) {
            for (j = 0; j < n_col; j++) {
                if (board[i][j] == 0) {
                    if (random_entry == emptyspaces) {
                        return i * n_col + j;
                    }
                    emptyspaces++;
                }
            }
        }
    }
    return -1;
}
