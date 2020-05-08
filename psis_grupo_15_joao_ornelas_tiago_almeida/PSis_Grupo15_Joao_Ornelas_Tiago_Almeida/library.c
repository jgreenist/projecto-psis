#include "clipboard.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/*
 *
 * Function used to connect the client to the local clipboard
 *
 *
 */
int clipboard_connect(char * clipboard_dir){

	struct sockaddr_un clipboard_addr;

	clipboard_addr.sun_family = AF_UNIX;
    strcpy(clipboard_addr.sun_path,clipboard_dir);

    int clipboard_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (clipboard_fd==-1){
        perror("socket() ");
    }
    puts("Socket created");

	//Connect to remote server
	if(connect(clipboard_fd, (struct sockaddr *)&clipboard_addr, sizeof(clipboard_addr))<0){
		perror("connect() ");
		return -1;
	}
	puts("Connected\n");

	return  clipboard_fd;

}


/*
 * Function used to copy data from a client to a clipboard region.
 * Receives a clipboard_id to connect to, a region id to store the data, a buffer and the size of the data.
 *
 */
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count){
    int n;

    //Creates the structure that holds the information that will affect the clipboard
	message m;
	m.action = COPY;                             //Action to be performed
    m.region = region;                           //region to be affected
    m.size = count;                              //Data size


    //Tests at library level if region is right
    if(m.region<0||m.region>9){
        printf("Invalid Region");
        return 0;
    }

    //Tests if message size is not negative
    if(count<0){
        printf("Invalid count value");
        return 0;
    }

    //Allocates the string that will store the structure with the info about the action that the user wants to perform
    char * msg_struct = malloc(sizeof(message));
	memcpy(msg_struct, &m, sizeof(message));

    //Sends the structure with the info about the action that the user wants to perform
    n = write(clipboard_id,msg_struct, sizeof(message));
	if (n<=0) {
		perror("write() 1");
		return 0;
	}

    //Sends the buffer with the data that the user wants to send
    n = write(clipboard_id,buf,count);
    printf("clipboard_copy.write: %d - region %d\n",n, m.region);
    if (n<=0) {
        perror("write() 2");
        return 0;
    }
    free(msg_struct);
	return n;
	
}
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count){
	int n;
    //Creates the structure that holds the information that will affect the clipboard
    message m;
	m.action = PASTE;                                         //Action to be performed
    m.region = region;                                       //region to be affected
    m.size =0;                                               //Data size

   // printf("clipboard_paste1: region: %d\n", m.region);
   // printf("clipboard_paste1: action: %d\n", m.action);
   // printf("clipboard_paste1: m.size: %d\n", m.size);

    //Tests at library level if region is right
    if(m.region<0||m.region>9){
        perror("invalid region: ");
        return 0;
    }

    //Tests if message size is not negative
    if(count<0){
        printf("Invalid count value");
        return 0;
    }

    //Allocates the string that will store the structure with the info about the action that the user wants to perform
    char * msg_struct = malloc(sizeof(message));
    memcpy(msg_struct, &m, sizeof(message));

    //Sends the structure with the info about the action that the user wants to perform
    n=write(clipboard_id,msg_struct, sizeof(message));
    if (n <= 0) {
        perror("write() ");
        return 0;
    }
    memset(msg_struct,'\0', sizeof(msg_struct));

    //printf("clipboard_paste.write (msg_struct) %d\n", n);


    printf("Clipboard Response: \n");

    //Reads the first response of the clipboard which will contain the size of the incoming message
    n = read(clipboard_id, msg_struct, sizeof(message));
    if (n <= 0) {
        perror("read() ");
        return 0;
    }

    //saves the info about the message copied from the clipboard and resets the msg_struct buf
    memcpy(&m, msg_struct, sizeof(message));
    memset(msg_struct,'\0', sizeof(msg_struct));

    printf("clipboard_paste: region: %d\n", m.region);
    printf("clipboard_paste: action: %d\n", m.action);
    printf("clipboard_paste: m.size: %d\n", m.size);
    //printf("clipboard_paste.read1 %d - %d\n",n, m.region);

    //allocates the buffer that will be used to receive the message with the necessary size
    char * recv_buf = (char*)malloc(sizeof(char)*m.size);

    //Reads the message from the clipboard
    n= read(clipboard_id, recv_buf, m.size);
    if (n <= 0) {
        perror("read() ");
        return 0;
    }
    memset((char*)buf,'\0', sizeof(buf));

   // printf("clipboard_paste.read2 %s - %d\n",recv_buf, n);


    //Stores only the data that the client buffer is able to support
    if (m.size > count) {
        memcpy((char*)buf, recv_buf, count);
    } else {
        memcpy((char*)buf, recv_buf, m.size);
    }
    //printf("buf %s\n",(char*)buf);

    //memcpy(buf, recv_buf, count);
    free(recv_buf);
    free(msg_struct);
    return m.size;




}

/*
 *
 * Closes the connection between the client and the clipboard
 *
 */
void clipboard_close(int clipboard_id){

    int n;
    message m;
    m.action = CLOSE;                                          //Action to be performed
    m.region = 0;                                              //region to be affected
    m.size =0;                                                 //Data size

    //Allocates the string that will store the structure with the info about the action that the user wants to perform
    char * msg_struct = malloc(sizeof(message));
    memcpy(msg_struct, &m, sizeof(message));

    //Sends the structure with the info about the action that the user wants to perform
    n=write(clipboard_id,msg_struct, sizeof(message));
    if (n <= 0) {
        perror("write() ");
        return;
    }



    free(msg_struct);
    return;

}



/*
 *
 * This function waits for a change on a certain region ( new copy), and when it happens the
 * new data in that region is copied to memory pointed by
 * buf.
 *
 */
int clipboard_wait(int clipboard_id, int region,void*buf,size_t count){
    int n;
    message m;
    m.action = WAIT;                                          //Action to be performed
    m.region = region;                                        //region to be affected
    m.size =0;                                                //Data size

    printf("clipboard_wait: region: %d\n", m.region);
    printf("clipboard_wait: action: %d\n", m.action);
    printf("clipboard_wait: m.size: %d\n", m.size);

    //Tests at library level if region is right
    if(m.region<0||m.region>9){
        perror("invalid region: ");
        return 0;
    }

    //Tests if message size is not negative
    if(count<0){
        printf("Invalid count value");
        return 0;
    }


    //Allocates the string that will store the structure with the info about the action that the user wants to perform
    char * msg_struct = malloc(sizeof(message));
    memcpy(msg_struct, &m, sizeof(message));


    //Sends the structure with the info about the action that the user wants to perform
    n=write(clipboard_id,msg_struct, sizeof(message));
    if (n <= 0) {
        perror("write() ");
        return 0;
    }
    memset(msg_struct,'\0', sizeof(msg_struct));

    //printf("clipboard_wait.write %d\n", n);


    printf("Clipboard Response: \n");


    //Reads the first response of the clipboard which will contain the size of the incoming message
    n = read(clipboard_id, msg_struct, sizeof(message));
    if (n <= 0) {
        perror("read() ");
        return 0;
    }

    //saves the info about the message copied from the clipboard and resets the msg_struct buf
    memcpy(&m, msg_struct, sizeof(message));
    memset(msg_struct,'\0', sizeof(msg_struct));

    //printf("clipboard_wait.read1 %d - %d\n",m.size, m.region);

    //allocates the buffer that will be used to receive the message with the necessary size
    char * recv_buf = (char*)malloc(sizeof(char)*m.size);


    //Reads the message from the clipboard
    n= read(clipboard_id, recv_buf, m.size);
    if (n <= 0) {
        perror("read() ");
        return 0;
    }
    memset((char*)buf,'\0', sizeof(buf));

    //printf("clipboard_wait.read2 %s - %d\n",recv_buf, n);

    //Stores only the data that the client buffer is able to support
    if (m.size > count) {
        memcpy((char*)buf, recv_buf, count);
    } else {
        memcpy((char*)buf, recv_buf, m.size);
    }
    printf("buf %s - %d\n",(char*)buf, m.size);

    free(recv_buf);
    free(msg_struct);
    return m.size;

}
