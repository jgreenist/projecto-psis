//
// Created by Ornelas on 07-05-2020.
//
typedef struct Fruits_Struct{
    sem_t               sem_fruit1;
    sem_t               sem_fruit2;
    sem_t               end_thread_fruit;
    pthread_t           thread_id;
    int 				fruit;
    int                 x;
    int 				y;
    struct Fruits_Struct *next;
}Fruits_Struct;


typedef struct IDListStruct{
    int 				ID;
    int                 socket;
    int 				rgb_r;
    int                 rgb_g;
    int                 rgb_b;
    int                 xp,yp;
    int                 xm,ym;
    int                 score;
    int                 superpower;
    struct IDListStruct *next;
}IDList;


/* Creates a new fd list node and puts it in the end of the existing fd list and returns head of the list*/
IDList *insert_new_ID(IDList *head, IDList * new);

/* Gets the fd of an element of a fd list */
int get_ID(IDList *elem);

/* Retrieves next node in the list */
IDList *get_next_ID(IDList *elem);

/* Removes node from fd list */
IDList *remove_ID(IDList *head, IDList *elem);

/* Gets the list_ID of a certain ID */
IDList *get_IDlist(IDList *head,int ID);

/* Closes all sockets */
void close_all_fd(IDList *head);

/* Frees the memory allocated to a fd list */
void free_ID_list(IDList *first);

//-----------------------------------------------------------------------------------------------//
//---------------------------------Functions for Fruit Lists-------------------------------------//
//-----------------------------------------------------------------------------------------------//


Fruits_Struct * insert_new_Fruit(Fruits_Struct *head, Fruits_Struct * new);

Fruits_Struct * get_fruit_list(Fruits_Struct *head,int fruit);

int get_fruit(Fruits_Struct *elem);
Fruits_Struct *get_next_fruit(Fruits_Struct *elem);
Fruits_Struct *remove_fruit_list(Fruits_Struct *head, Fruits_Struct *elem);
void free_fruit_list(Fruits_Struct *first);



