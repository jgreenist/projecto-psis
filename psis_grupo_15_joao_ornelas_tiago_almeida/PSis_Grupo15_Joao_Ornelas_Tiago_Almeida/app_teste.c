#include "clipboard.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>


/*
 * Client program, connects to clipboard by calling clipboard_connect() and provides the interface for
 the user to interact with the clipboard
 *
 */

int main(int argc, char const *argv[]) {


    char buf[20];
    int region = -1;                          //clipboard region that the user wants to affect
    char user_action[10];                     //action entered by the user
    char action[10];                          //used to compare user action with clipboard functionalities
    int cmp;                                  //result of user_action and action comparison
    int size;                                 //data size that the user wants to receive
    int close = 0;                            //closing the program flag

    int fd = clipboard_connect(SOCKADDR);     //connect to clipboard (UNIX)
    if(fd==-1){
        printf("Unable to connect to clipboard. \n");
        exit(-1);
    }

    /*
     * Client loop. Compares user_action with the different possible actions.
     * Possible actions: Paste, Copy, Wait, Close
     *
     */
    while (close!=1) {
        printf("\nFor paste action enter \"paste\", for copy enter \"copy\", for wait enter \"wait\". To close client enter \"close\"\n");

        //Copy action
        fgets(user_action, sizeof(user_action),stdin);
        strcpy(action, "copy\n");

        cmp=strcmp(user_action,action);
        if(cmp==0){                                                    //If user action is copy
            printf("Enter message : ");
            if(fgets(buf, sizeof(buf),stdin)){
                char *p;
                if(p=strchr(buf, '\n')){                                //check exist newline
                    *p = 0;
                } else {
                    scanf("%*[^\n]");scanf("%*c");                      //clear upto newline
                }
            }
            while (region<0||region>9){
                printf("Enter region : \n");
                scanf("%d", &region);
                if (region>9||region<0) {
                    printf("Please enter a valid region!\n");
                    scanf("%d", &region);
                }

            }
            if(clipboard_copy(fd,region, (void*)buf, sizeof(buf))==0){   //Calls clipboard copy with region selected by user and message casted as void*
                printf("Error in clipboard_copy(). Please try again.\n");
            };
            getchar();
            memset(buf,'\0', sizeof(buf));                              //Resets buf

        }

        //paste action
        strcpy(action, "paste\n");
        if(strcmp(user_action,action)==0){                               //If user action is copy
            while (region<0||region>9){
                printf("Enter region : ");
                scanf("%d", &region);
                if (region>9||region<0) {
                    printf("Please enter a valid region!\n");
                    scanf("%d", &region);
                }
            }
            getchar();
            printf("Enter size of data wanted: ");                      //Allows user to choose the size of data that he wants to save
            scanf("%d", &size);
            if(size> sizeof(buf)){
                size= sizeof(buf);
            }

            //Calls clipboard_paste to the region that user specified and asks for a certain size of data.
            if(clipboard_paste(fd,region,(void*)buf, size)==0){
                printf("Error in clipboard_paste(). Please try again.\n");

            }
            printf("Data received: %s\n",(char*)buf);                   //Prints data received via paste
            getchar();
            memset(buf,'\0', sizeof(buf));                              //Resets buf

        }

        //wait action
        strcpy(action, "wait\n");                                       //If user action is wait
        if(strcmp(user_action,action)==0){
            while (region<0||region>9){
                printf("Enter region : ");
                scanf("%d", &region);
                if (region>9||region<0) {
                    printf("Please enter a valid region!\n");
                    scanf("%d", &region);
                }
            }
            getchar();
            printf("Enter size of data wanted: ");                      //Allows user to choose the size of data that he wants to save
            scanf("%d", &size);
            if(size> sizeof(buf)){
                size= sizeof(buf);
            }
            //Calls clipboard_wait to the region that user specified and asks for a certain size of data.
            printf("size %d\n", size );

            printf("region %d\n", region );
            if(clipboard_wait(fd,region,(void*)buf, size)==0){
                printf("Error in clipboard_wait(). Please try again.\n");
            }
            printf("Data received: %s\n",(char*)buf);                   //Prints data received via paste

            getchar();
            memset(buf,'\0', sizeof(buf));                              //Resets buf

        }
        region=-1;                                                      //reset region

        //close client
        strcpy(action, "close\n");
        if(strcmp(user_action,action)==0){
            clipboard_close(fd);
            close=1;
        }
    }

    printf("Thank you for using our clipboard service!");
    exit(0);
}
