//
// Created by joao Ornelas on 07-05-2020.
//

#include <unistd.h>
#include <stdlib.h>
#include "list_ID.h"



/* Creates a new fd list node and puts it in the end of the existing fd list and returns head of the list*/
IDList *insert_new_ID(IDList *head, IDList * new){
    //IDList * new;
    IDList * aux;

    /* Memory allocation */
    //new = (IDList *) malloc(sizeof(IDList));

    /* Check memory allocation errors */
    //if(new == NULL)
    //    return NULL;

    /* initialize new element */
    //new->ID = ID;
    //new->socket=socket;
    //new->rgb_r = rgb_r;
    //new->rgb_g = rgb_g;
    //new->rgb_b = rgb_b;
    //new->next = NULL;

    /* if list is empty, return new element */
    if(head == NULL){
        return new;
    }

    /* point aux to last element */
    aux = head;
    while(aux->next != NULL){
        aux = aux->next;
    }

    /* insert new in the end */
    aux->next = new;

    return head;
}

/* Gets the list_ID of a certain ID */
IDList *get_IDlist(IDList *head,int ID){
    /* Check if node is not empty */
    /* if list is empty, return new element */
    IDList * aux;
    if(head == NULL){
        return NULL;
    }
    /* point aux to last element */
    aux = head;
    while(aux->next != NULL || aux->ID!=ID){
        aux = aux->next;
    }
    return aux;
}


/* Gets the fd of an element of a fd list */
int get_ID(IDList *elem){
    /* Check if node is not empty */
    if(elem == NULL)
        return -1;

    return elem->ID;
}

/* Retrieves next node in the list */
IDList *get_next_ID(IDList *elem){
    /* Check if node is not empty */
    if(elem == NULL)
        return NULL;

    return elem->next;
}

/* Removes node from fd list */
IDList *remove_ID(IDList *head, IDList *elem){
    IDList *aux;

    /* empty list */
    if(head == NULL)
        return NULL;

    aux = head;

    /* element is head */
    if(elem == head){
        /* it is the only element in the list */
        if(elem->next == NULL){
            free(elem);
            return NULL;
        }
        aux = elem->next;
        free(elem);
        return aux;
    }

    /* search for element in the list */
    while(aux->next != elem){
        aux = aux->next;
    }

    /* update pointers, close socket and remove*/
    aux->next = elem->next;
    free(elem);

    return head;
}

/* Closes all sockets */
void close_all_fd(IDList *head){
    IDList *aux = head;

    while(aux != NULL){
        close(aux->socket);
        aux = aux->next;
    }
}

/* Frees the memory allocated to a fd list */
void free_ID_list(IDList *first){
    IDList * next;
    IDList * aux;

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