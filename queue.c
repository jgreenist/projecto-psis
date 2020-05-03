//
// Created by tiago on 29-05-2018.
//

#include <unistd.h>
#include <stdlib.h>
#include "queue.h"



/* Creates a new fd list node and puts it in the end of the existing fd list and returns head of the list*/
fdList *insert_new_fd(fdList *head, int fd){
    fdList * new;
    fdList * aux;

    /* Memory allocation */
    new = (fdList *) malloc(sizeof(fdList));

    /* Check memory allocation errors */
    if(new == NULL)
        return NULL;

    /* initialize new element */
    new->fd = fd;
    new->next = NULL;

    /* if list is empty, return new element */
    if(head == NULL){
        return new;
    }

    /* point aux to last element */
    aux = head;
    while(aux->next != NULL){
        aux = aux->next;
    }

    /* inser new in the end */
    aux->next = new;

    return head;
}

/* Gets the fd of an element of a fd list */
int get_fd(fdList *elem){
    /* Check if node is not empty */
    if(elem == NULL)
        return -1;

    return elem->fd;
}

/* Retrieves next node in the list */
fdList *get_next_fd(fdList *elem){
    /* Check if node is not empty */
    if(elem == NULL)
        return NULL;

    return elem->next;
}

/* Removes node from fd list */
fdList *remove_fd(fdList *head, fdList *elem){
    fdList *aux;

    /* empty list */
    if(head == NULL)
        return NULL;

    aux = head;

    /* element is head */
    if(elem == head){
        /* it is the only element in the list */
        if(elem->next == NULL){
            close(elem->fd);
            free(elem);
            return NULL;
        }
        aux = elem->next;
        close(elem->fd);
        free(elem);
        return aux;
    }

    /* search for element in the list */
    while(aux->next != elem){
        aux = aux->next;
    }

    /* update pointers, close socket and remove*/
    close(elem->fd);
    aux->next = elem->next;
    free(elem);

    return head;
}

/* Closes all sockets */
void close_all_fd(fdList *head){
    fdList *aux = head;

    while(aux != NULL){
        close(aux->fd);
        aux = aux->next;
    }
}

/* Frees the memory allocated to a fd list */
void free_fd_list(fdList *first){
    fdList * next;
    fdList * aux;

    /* Cycle from the first to the last element                     */
    for(aux = first; aux != NULL; aux = next)
    {
        /* Keep trace of the next node                                */
        next = aux->next;

        /* Free current node                                          */
        free(aux);
    }

    return;
}