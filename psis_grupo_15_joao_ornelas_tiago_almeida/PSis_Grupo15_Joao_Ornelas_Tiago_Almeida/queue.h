//
// Created by tiago on 29-05-2018.
//


typedef struct fdListStruct{
	int 				ID;
    int 				socket_id;
    struct fdListStruct *next;
}fdList;


/* Creates a new fd list node and puts it in the end of the existing fd list and returns head of the list*/
fdList *insert_new_fd(fdList *head, int fd);

/* Gets the fd of an element of a fd list */
int get_fd(fdList *elem);

/* Retrieves next node in the list */
fdList *get_next_fd(fdList *elem);

/* Removes node from fd list */
fdList *remove_fd(fdList *head, fdList *elem);

/* Closes all sockets */
void close_all_fd(fdList *head);

/* Frees the memory allocated to a fd list */
void free_fd_list(fdList *first);


