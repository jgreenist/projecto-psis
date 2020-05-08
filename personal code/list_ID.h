//
// Created by Ornelas on 07-05-2020.
//


typedef struct IDListStruct{
    int 				ID;
    int                 socket;
    int 				rgb_r;
    int                 rgb_g;
    int                 rgb_b;
    struct IDListStruct *next;
}IDList;


/* Creates a new fd list node and puts it in the end of the existing fd list and returns head of the list*/
IDList *insert_new_ID(IDList *head, int ID, int socket,int rgb_r, int rgb_g,int rgb_b);

/* Gets the fd of an element of a fd list */
int get_ID(IDList *elem);

/* Retrieves next node in the list */
IDList *get_next_ID(IDList *elem);

/* Removes node from fd list */
IDList *remove_ID(IDList *head, IDList *elem);

/* Closes all sockets */
void close_all_fd(IDList *head);

/* Frees the memory allocated to a fd list */
void free_ID_list(IDList *first);


