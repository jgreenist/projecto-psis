#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>

#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include "queue.h"
#include "clipboard.h"


// Global variables
// type used for the connection with the Clipboard higher in tree
typedef struct Connection {
    int fd;
    struct sockaddr_in addr;
} Connection;

//Used for the connections with the lower Clipboards in tree
//creates a list for the fd's and a lock to use in the reads
// and writes
pthread_rwlock_t fd_list;
fdList *fds;

//Initialization of the clipboard and a vector for the size of each entry
char ** clipboard;
int clipboardSize[10];

//User for the writing and reading from the clipboard
//and it is created one for each region in the clipoard
pthread_rwlock_t rwlock[10];

//User for the operation wait and it is created one for each region in the clipoard
pthread_cond_t cond[10];
pthread_mutex_t mux[10];

//Used for the connection with the Clipboard higher in tree
pthread_rwlock_t ClipConnected;
int clipboardConnected=0;

//Used for the connection with the Clipboard higher in tree
//is writing to only in the beggining of the program before
//any other thread has the chance to read it
Connection clip_conn;

int debug;



/* A Clip_thread is created for each of the clients connected to clipboard
 * Input: fd of the app that has been connected
 *
 * Here we are capable of resolving the four requests of the app: 1)copy2)paste3)wait4)close
*/
void *clip_thread(void*varg){

    //Initilization of the local variables
    int *client_fd1=(int*) varg;
    int client_fd=*client_fd1;


    char * data = (char *)malloc(sizeof(message));
    message m;
    char * tmp_buf;

    char empty_region[21];
    strcpy(empty_region, "Empty region!");
    empty_region[20]='\0';

    int Connected;
    int curr_read;
    int read_size;
    int n;

    //Receive client message and check for errors
    while ((read_size = read(client_fd, data, sizeof(message))) > 0) {
        curr_read=0;
        while(read_size < sizeof(message)){
            curr_read=read(client_fd, data+read_size, sizeof(message)-read_size);
            read_size=curr_read+read_size;
        }

        memcpy(&m, data, sizeof(message));
        printf("app_thread: region: %d\n", m.region);
        printf("app_thread: action: %d\n", m.action);
        printf("app_thread: m.size: %d\n", m.size);

        // check for the app request
        if (m.action == COPY) {
            //read the information received and check for errors
            tmp_buf = (char *) malloc(m.size);
            if ((read_size=read(client_fd, tmp_buf, m.size)) <= 0) {
                printf("app_thread - read(): Unable to read\n");
                free(tmp_buf);
                continue;
            }

            // check if all the information was received
            curr_read=0;
            while(read_size < sizeof(m.size)){
                curr_read=read(client_fd, tmp_buf+m.size, m.size-read_size);
                read_size=curr_read+read_size;
            }

            //If Clipboard is not the highest in tree then write up
            pthread_rwlock_rdlock(&ClipConnected);
            Connected=clipboardConnected;
            pthread_rwlock_unlock(&ClipConnected);

            if(Connected==1) {

                //write and check for errors
                //if the connection is not available change the status to not connected
                if(n=write(clip_conn.fd, data, sizeof(message))==0){
                    perror("app_thread: write()1:");

                    pthread_rwlock_wrlock(&ClipConnected);
                    clipboardConnected=0;
                    pthread_rwlock_unlock(&ClipConnected);

                }

                //check for errors if the connection is not available change the status to not connected
                int error = 0;
                socklen_t len = sizeof (error);
                int retval = getsockopt (clip_conn.fd, SOL_SOCKET, SO_ERROR, &error, &len);
                if (retval != 0) {
                    /* there was a problem getting the error code */
                    fprintf(stderr, "app_thread: error getting socket error code: %s\n", strerror(retval));
                    pthread_rwlock_wrlock(&ClipConnected);
                    clipboardConnected=0;
                    pthread_rwlock_unlock(&ClipConnected);
                }
                if (error != 0) {
                    /* socket has a non zero error status */
                    fprintf(stderr, "app_thread: socket error: %s\n", strerror(error));
                    pthread_rwlock_wrlock(&ClipConnected);
                    clipboardConnected=0;
                    pthread_rwlock_unlock(&ClipConnected);
                }


                //second read if the connection is still available
                pthread_rwlock_rdlock(&ClipConnected);
                Connected=clipboardConnected;
                pthread_rwlock_unlock(&ClipConnected);

                if(Connected==1)  {
                    if(write(clip_conn.fd, tmp_buf, m.size)<=0){
                        perror("app_thread:  write()2:");
                        pthread_rwlock_wrlock(&ClipConnected);
                        clipboardConnected=0;
                        pthread_rwlock_unlock(&ClipConnected);

                    }
                }
            }



            //If not connect to a higher clipboard in the tree send to all clipboard under this one
            pthread_rwlock_rdlock(&ClipConnected);
            Connected=clipboardConnected;
            pthread_rwlock_unlock(&ClipConnected);

            if(Connected==0){
                //write in memory and write down to all
                pthread_rwlock_wrlock(&rwlock[m.region]);
                if (clipboard[m.region] != NULL) {
                    free(clipboard[m.region]);
                }

                clipboard[m.region] = (char *) malloc(m.size);
                memset(clipboard[m.region], '\0', m.size);
                memcpy(clipboard[m.region], tmp_buf, m.size);
                clipboardSize[m.region] = m.size;
                pthread_rwlock_unlock(&rwlock[m.region]);

                //send a signal to all client waiting in this region
                pthread_mutex_lock(&mux[m.region]);
                pthread_cond_broadcast(&cond[m.region]);
                pthread_mutex_unlock(&mux[m.region]);

                //get the lists of fd's
                pthread_rwlock_rdlock(&fd_list);
                fdList *aux = fds;
                pthread_rwlock_unlock(&fd_list);

                //send the information to all clipboard in the list of fd's
                while (aux != NULL) {

                    //check if the cliboard under is available if not remove from list
                    //and continue
                    int error = 0;
                    socklen_t len = sizeof (error);
                    int retval = getsockopt (get_fd(aux), SOL_SOCKET, SO_ERROR, &error, &len);
                    if (retval != 0) {
                        /* there was a problem getting the error code */
                        fprintf(stderr, "app_thread: error getting socket error code: %s\n", strerror(retval));
                        pthread_rwlock_wrlock(&fd_list);
                        fds=remove_fd(fds,aux);
                        pthread_rwlock_unlock(&fd_list);

                        aux = aux->next;
                        continue;
                    }
                    if (error != 0) {
                        /* socket has a non zero error status */
                        fprintf(stderr, "app_thread: socket error: %s\n", strerror(error));
                        pthread_rwlock_wrlock(&fd_list);
                        fds=remove_fd(fds,aux);
                        pthread_rwlock_unlock(&fd_list);
                        
                        aux = aux->next;
                        continue;
                    }


                    if (write(get_fd(aux), data, sizeof(message)) <= 0) {
                        perror("app_thread: write()1:");

                    }
                    if (write(get_fd(aux), tmp_buf, m.size) <= 0) {
                        perror("app_thread: write()2:");

                    }
                    aux = aux->next;
                }
            }
            //continue work

            free(tmp_buf);
            memset(data,'\0', sizeof(data));

        // check for the app request
        } else if (m.action == PASTE) {
            //check if region not null
            if (clipboard[m.region] != NULL) {
                // read from that region
                pthread_rwlock_rdlock(&rwlock[m.region]);
                m.size = clipboardSize[m.region];
                tmp_buf = (char *) malloc(m.size);
                memcpy(data, &m, sizeof(message));
                memcpy(tmp_buf, clipboard[m.region], m.size);
                pthread_rwlock_unlock(&rwlock[m.region]);

                //write back with the requested information
                n=write(client_fd, data, sizeof(message));
                if(n<=0){
                    perror("app_thread: write1() ");
                }
                write(client_fd, tmp_buf, m.size);
                if(n<=0){
                    perror("app_thread: write2() ");
                }

                free(tmp_buf);
                memset(data,'\0', sizeof(data));

            } else {
                //if region null send that is empty
                pthread_rwlock_rdlock(&rwlock[m.region]);
                m.size = sizeof(empty_region);
                memcpy(data, &m, sizeof(message));
                pthread_rwlock_unlock(&rwlock[m.region]);

                write(client_fd, data, sizeof(message));
                write(client_fd, empty_region, sizeof(empty_region));
                printf("app_thread: %s \n", empty_region);

                memset(data,'\0', sizeof(data));


            }
        // check for the app request
        } else if(m.action==WAIT){
            //start waiting for other app to change a specific region
            pthread_mutex_lock(&mux[m.region]);
            pthread_cond_wait(&cond[m.region], &mux[m.region]);
            pthread_mutex_unlock(&mux[m.region]);

            //check if region is empty
            if (clipboard[m.region] != NULL) {
                //read region
                pthread_rwlock_rdlock(&rwlock[m.region]);
                m.size = clipboardSize[m.region];
                tmp_buf = (char *) malloc(m.size);
                memcpy(data, &m, sizeof(message));
                memcpy(tmp_buf, clipboard[m.region], sizeof(message));
                pthread_rwlock_unlock(&rwlock[m.region]);

                //send information back to app
                n=write(client_fd, data, sizeof(message));
                if(n<=0){
                    perror("app_thread: write1() ");
                }
                write(client_fd, tmp_buf, m.size);
                if(n<=0){
                    perror("app_thread: write2() ");
                }

                free(tmp_buf);
                memset(data,'\0', sizeof(data));

            } else {
                //if region null send that is empty
                pthread_rwlock_rdlock(&rwlock[m.region]);
                m.size = sizeof(empty_region);
                memcpy(data, &m, sizeof(message));
                pthread_rwlock_unlock(&rwlock[m.region]);

                write(client_fd, data, sizeof(message));
                write(client_fd, empty_region, sizeof(empty_region));
                printf("app_thread: %s \n", empty_region);

                memset(data,'\0', sizeof(data));


            }
        // check for the app request
        } else if (m.action == CLOSE){
            //close socket, free data and close thread
            puts("app_thread: APP disconnected");
            close(client_fd);
            free(data);
            pthread_exit(NULL);
        }


    }
    if (read_size == 0) {
        //close socket, free data and close thread
        puts("app_thread: APP disconnected");
        close(client_fd);
        free(data);
        pthread_exit(NULL);
    } else if (read_size == -1) {
        //close socket, free data and close thread
        perror("app_thread: read() ");
        close(client_fd);
        free(data);
        pthread_exit(NULL);
    }
    //close socket, free data and close thread
    close(client_fd);
    free(data);
    pthread_exit(NULL);


}

/* A mainConnection_thread is created for the connection with the upper Clipboard in tree
 * Input: Information of upper Clipboard in tree by the global variable clip_conn
 *
 * Here we listen to the upper Clipboard and write that information in the memory of the
 * Clipboard and send it downwards to all Clipboards listening to this clipboard
*/
void *mainConnection_thread(void*varg){
    //Initilization of local variables
    message m;
    char * tmp_buf;
    char * data = (char *)malloc(sizeof(message));

    int read_size;

    //read from the upper Clipboard in the tree
    while ((read_size=read(clip_conn.fd, data, sizeof(message))) > 0) {
        printf("mainConnection_thread: Reading to write in memory and to send down\n");
        memcpy(&m,data, sizeof(message));


        tmp_buf = (char *) malloc(m.size);
        if (read_size=read(clip_conn.fd, tmp_buf, m.size) <= 0) {
            continue;
        }

        //write in the memory
        pthread_rwlock_wrlock(&rwlock[m.region]);
        if (clipboard[m.region] != NULL) {
            free(clipboard[m.region]);
        }
        clipboard[m.region] = (char *) malloc(m.size);
        memset(clipboard[m.region], '\0', m.size);
        memcpy(clipboard[m.region], tmp_buf, m.size);
        clipboardSize[m.region] = m.size;
        pthread_rwlock_unlock(&rwlock[m.region]);

        //send a signal to all client waiting in this region
        pthread_mutex_lock(&mux[m.region]);
        pthread_cond_broadcast(&cond[m.region]);
        pthread_mutex_unlock(&mux[m.region]);



        //write down to all clipboard listening to this one
        pthread_rwlock_rdlock(&fd_list);
        fdList *aux = fds;
        pthread_rwlock_unlock(&fd_list);

        while(aux!=NULL){
            //check if the cliboard under is available if not remove from list
            //and continue
			int error = 0;
            socklen_t len = sizeof (error);
            int retval = getsockopt (get_fd(aux), SOL_SOCKET, SO_ERROR, &error, &len);
            if (retval != 0) {
                  /* there was a problem getting the error code */
                fprintf(stderr, "mainConnection_thread: error getting socket error code: %s\n", strerror(retval));
                pthread_rwlock_wrlock(&fd_list);
                fds=remove_fd(fds,aux);
                pthread_rwlock_unlock(&fd_list);
                        
                aux = aux->next;
                continue;
             }

             if (error != 0) {
             /* socket has a non zero error status */
                fprintf(stderr, "mainConnection_thread: socket error: %s\n", strerror(error));
                pthread_rwlock_wrlock(&fd_list);
                fds=remove_fd(fds,aux);
                pthread_rwlock_unlock(&fd_list);
                        
                aux = aux->next;
                continue;
            }

            //write to the cliboard under and check for errors
            if(write(get_fd(aux), data, sizeof(message))<=0){
                perror("mainConnection_thread: write()1:");
            }
            if(write(get_fd(aux), tmp_buf, m.size)<=0){
                perror("mainConnection_thread: write()2:");
            }
            aux=aux->next;

        }
        free(tmp_buf);
        memset(data,'\0', sizeof(data));
    }
    if (read_size == 0) {
        //close socket, free data and close thread
        puts("mainConnection_thread: Upper Clipboard disconnected");
        close(clip_conn.fd);
        free(data);
        pthread_exit(NULL);
    } else if (read_size == -1) {
        //close socket, free data and close thread
        perror("mainConnection_thread: read() ");
        close(clip_conn.fd);
        free(data);
        pthread_exit(NULL);
    }
    //close socket, free data and close thread
    close(clip_conn.fd);
    free(data);
    pthread_exit(NULL);
}


/* A interClip_thread is created for the each of the connections with a Clipboard
 * down in the tree
 * Input: fd of a clipboard down in the tree
 *
 * Here we listen to one of the Clipboard down and do one of two things:
 * 1) Clipboard if the highest in the tree write that information in the memory of the
 * Clipboard and send it downwards to all Clipboards listening to this clipboard
 * 2) Clipboard if not the highest in the tree send that information to the upper
 * connection
*/

void *interClip_thread(void*varg){

    int *tcpclient_fd1=(int*) varg;
    int tcpclient_fd=*tcpclient_fd1;

    //Initialization of local variables
    message m;
    char * tmp_buf;
    char * data = (char *)malloc(sizeof(message));
    int Connected;


    int read_size;
    int n;
    //read from Clipboard down in the tree
    while ((read_size=read(tcpclient_fd, data, sizeof(message))) > 0) {

        printf("interClip_thread: reading\n");
        memcpy(&m,data, sizeof(message));
        tmp_buf = (char *) malloc(m.size);
        if ((read_size=read(tcpclient_fd, tmp_buf, m.size)) <= 0) {
            continue;
        }

        pthread_rwlock_rdlock(&ClipConnected);
        Connected=clipboardConnected;
        pthread_rwlock_unlock(&ClipConnected);

        //write up to upper Clipboard if connected
        if(Connected==1){

            //write up and check for errors and put in disconnected mode if true
            if(n=write(clip_conn.fd, data, sizeof(message))==0){
                perror("interClip_thread: write()1:");
                clipboardConnected=0;
            }

            //check for errors and put in disconnected mode if true
            int error = 0;
            socklen_t len = sizeof (error);
            int retval = getsockopt (clip_conn.fd, SOL_SOCKET, SO_ERROR, &error, &len);
            if (retval != 0) {
                /* there was a problem getting the error code */
                fprintf(stderr, "interClip_thread: error getting socket error code: %s\n", strerror(retval));
                pthread_rwlock_wrlock(&ClipConnected);
                clipboardConnected=0;
                pthread_rwlock_unlock(&ClipConnected);
            }

            if (error != 0) {
                /* socket has a non zero error status */
                fprintf(stderr, "interClip_thread: socket error: %s\n", strerror(error));
                pthread_rwlock_wrlock(&ClipConnected);
                clipboardConnected=0;
                pthread_rwlock_unlock(&ClipConnected);
            }

            pthread_rwlock_rdlock(&ClipConnected);
            Connected=clipboardConnected;
            pthread_rwlock_unlock(&ClipConnected);

            //write up and check for errors and put in disconnected mode if true
            if(Connected==1)  {
                if(write(clip_conn.fd, tmp_buf, m.size)<=0){
                    perror("interClip_thread: write()2:");
                    pthread_rwlock_wrlock(&ClipConnected);
                    clipboardConnected=0;
                    pthread_rwlock_unlock(&ClipConnected);

                }

            }



        }

        pthread_rwlock_rdlock(&ClipConnected);
        Connected=clipboardConnected;
        pthread_rwlock_unlock(&ClipConnected);

        //write in the memory and write down if the highest in the tree
        if(Connected==0){
            pthread_rwlock_wrlock(&rwlock[m.region]);
            if (clipboard[m.region] != NULL) {
                free(clipboard[m.region]);
            }
            clipboard[m.region] = (char *) malloc(m.size);
            memset(clipboard[m.region], '\0', m.size);
            memcpy(clipboard[m.region], tmp_buf, m.size);
            clipboardSize[m.region] = m.size;
            pthread_rwlock_unlock(&rwlock[m.region]);

            //send a signal to all client waiting in this region
            pthread_mutex_lock(&mux[m.region]);
            pthread_cond_broadcast(&cond[m.region]);
            pthread_mutex_unlock(&mux[m.region]);


            //write down to all clipboard listening to this one
            //get the lists of fd's
            pthread_rwlock_rdlock(&fd_list);
            fdList *aux = fds;
            pthread_rwlock_unlock(&fd_list);

            while(aux!=NULL){
                //check if the cliboard under is available if not remove from list
                //and continue
				int error = 0;
				socklen_t len = sizeof (error);
				int retval = getsockopt (get_fd(aux), SOL_SOCKET, SO_ERROR, &error, &len);
				if (retval != 0) {
                  /* there was a problem getting the error code */
					fprintf(stderr, "interClip_thread: error getting socket error code: %s\n", strerror(retval));
					pthread_rwlock_wrlock(&fd_list);
					fds=remove_fd(fds,aux);
					pthread_rwlock_unlock(&fd_list);
                        
					aux = aux->next;
					continue;
				}

				if (error != 0) {
				/* socket has a non zero error status */
					fprintf(stderr, "interClip_thread: socket error: %s\n", strerror(error));
					pthread_rwlock_wrlock(&fd_list);
					fds=remove_fd(fds,aux);
					pthread_rwlock_unlock(&fd_list);
                        
					aux = aux->next;
					continue;
				}
                //write to the cliboard under and check for errors
                if(write(get_fd(aux), data, sizeof(message))<=0){
                    perror("interClip_thread: write()1:");
                }
                if(write(get_fd(aux), tmp_buf, m.size)<=0){
                    perror("interClip_thread: write()2:");
                }
                aux=aux->next;
            }
        }
        free(tmp_buf);
        memset(data,'\0', sizeof(data));
    }
    if (read_size == 0) {
        puts("interClip_thread: Downwards Clipboard disconnected");
        //close socket, free data and close thread
        close(tcpclient_fd);
        free(data);
        pthread_exit(NULL);
    } else if (read_size == -1) {
        //close socket, free data and close thread
        perror("interClip_thread: read() ");
        close(tcpclient_fd);
        free(data);
        pthread_exit(NULL);
    }
    //close socket, free data and close thread
    close(tcpclient_fd);
    free(data);
    pthread_exit(NULL);


}

/* A server_thread is created to accept connections from  Clipboards
* down in the tree
* Input: Information about the tcp server created in the form of the
* struct Connection
*
* Accepts connections from  Clipboards down in the tree and send the
* information already in memory downwards
*/

void *server_thread(void*varg){


    Connection *server1 = (Connection *) varg;
    Connection server = *server1;
    int tcpclient_fd;

    //Initialization of local variables
    message m;
    char * data=(char *)malloc(sizeof(message));
    char * tmp_buf;

    struct sockaddr_un clipboard_addr;
    int clipboard_addr_len = sizeof(clipboard_addr);

    //wait for incoming connections
    while(1) {
        puts("server_thread: Waiting for incoming connections...");
        if ((tcpclient_fd = accept(server.fd, (struct sockaddr *)&server.addr, &clipboard_addr_len)) == -1) {
            perror("server_thread: Accept() ");
            continue;
        } else {
            puts("server_thread: Connection accepted");

            //creates the threads responsable for the connections with the clipboard
            //down in the tree
            pthread_t interClip;
            pthread_create(&interClip,NULL,interClip_thread,&tcpclient_fd);

            // add fd to list
            pthread_rwlock_wrlock(&fd_list);
            fds=insert_new_fd(fds,tcpclient_fd);
            printf("server_thread: fds: %d\n",fds->fd);
            pthread_rwlock_unlock(&fd_list);

            //Send the information already in memory downwards
			int i=0;
            for(i=0; i<10;++i){
                m.action=COPY;
                m.region=i;

                pthread_rwlock_rdlock(&rwlock[i]);
                if((m.size=clipboardSize[i])==0){
                    pthread_rwlock_unlock(&rwlock[i]);
                    continue;
                }
                tmp_buf = (char *) malloc(m.size);
                memcpy(data, &m, sizeof(message));
                memcpy(tmp_buf, clipboard[m.region], m.size);
                pthread_rwlock_unlock(&rwlock[m.region]);

                if(write(tcpclient_fd, data, sizeof(message))<=0){
                    perror("server_thread: write()1:");
                }

                if(write(tcpclient_fd, tmp_buf, m.size)<=0){
                    perror("server_thread: write()2:");
                }

                free(tmp_buf);
                memset(data,'\0', sizeof(data));
            }

        }

    }

    exit(0);
}

/* A main thread is created to create the threads responsable for each
 * of the connection with each app, the connection with upper clipboard
 * and the thread responsable to accept connections from other clipboards
 *
 * Input: Connection mode or Disconnected mode
*/




int main(int argc, char **argv) {


    //Initialization of all sync mechanism
	int i = 0;
    for ( i = 0; i < 10; i++) {
        pthread_rwlock_init(&rwlock[i], NULL);
        pthread_mutex_init(&mux[i], NULL);
        pthread_cond_init(&cond[i], NULL);

    }

    pthread_rwlock_init(&fd_list, NULL);
    pthread_rwlock_init(&ClipConnected, NULL);

    //Creating TCP client -> Connecting to other clipboard
  

    if (argc > 2) {
        if (strcmp("-c", argv[1]) == 0 && argc > 2) {
            clip_conn.fd = socket(AF_INET, SOCK_STREAM, 0);
            clip_conn.addr.sin_family = AF_INET;
            if (inet_aton(argv[2], &(clip_conn.addr.sin_addr)) == 0) {
                printf("main: error translating\n");
                return -1;
            }
            int port = atoi(argv[3]);
            clip_conn.addr.sin_port = htons(port);
            int clip_conn_addrlen = sizeof(clip_conn.addr);

            //Connect
            if (connect(clip_conn.fd, (struct sockaddr *) &(clip_conn.addr),
                        sizeof(clip_conn.addr)) == -1) {
                printf("main: error connecting to clipboard\n");
                return -1;
            } else {
                //Variable to track if clipboard is connected to other clipboard or not (up direction)
                clipboardConnected = 1;
            }
        }
        if (strcmp("-d", argv[4]) == 0 && argc > 2) {
            debug=1;
        }
    }


    //Creating TCP Clipboard server.
    Connection server;
    server.fd = socket(AF_INET, SOCK_STREAM, 0);
    server.addr.sin_family = AF_INET;
    server.addr.sin_addr.s_addr = htonl(INADDR_ANY);
    int max, min, port;
    max=8100;
    min=8000;
    srand(time(NULL));
    port = (rand())%(max+1-min)+min;
    server.addr.sin_port = htons(port);

    if (bind(server.fd, (struct sockaddr*)&(server.addr), sizeof(server.addr)) == -1) {
        perror("main: bind() ");
        return -1;
    }
    puts("main: bind() successful");

    if (listen(server.fd, 5)==-1){
        perror("main:listen() ");
        return -1;
    }

    printf("main: This clipboard is listening on port: %d\n",port);


    //Creating queue to store fds of incoming clipboard connections

    fds=NULL;


    // Creating sockets for client connections

    struct sockaddr_un clipboard_addr;
    clipboard_addr.sun_family = AF_UNIX;
    strcpy(clipboard_addr.sun_path, SOCKADDR);

    unlink(SOCKADDR);
    int clipboard_fd = socket(AF_UNIX, SOCK_STREAM,0);
    if (clipboard_fd == -1){
        perror("main: socket() ");
        return -1;
    }
    puts("main: Socket created");

    if (bind(clipboard_fd, (struct sockaddr*)&clipboard_addr, sizeof(clipboard_addr)) == -1) {
        perror("main:bind() ");
        return -1;
    }
    puts("main: bind() successful");

    if (listen(clipboard_fd, 5)==-1){
        perror("main: listen() ");
        return 1;
    }

    //Initialization of clipboard memory
    int clipboard_addr_len = sizeof(clipboard_addr);
    clipboard =(char **)malloc(10* sizeof(char*));


    for ( i = 0; i < 10; i++) {
        clipboard[i] = NULL;
        clipboardSize[i] = 0;
    }

    //Initialization of mainConnection_thread for the connection with the upper clipboard
    pthread_t mainCoin;
    pthread_create(&mainCoin,NULL,mainConnection_thread,NULL);

    //Initialization of server_thread responsable for acception clipboard connections
    pthread_t server_conn;
    pthread_create(&server_conn,NULL,server_thread,&server);

    int client_fd;

    while(1) {
        puts("main: Waiting for incoming connections...");
        if ((client_fd = accept(clipboard_fd, (struct sockaddr *) &clipboard_addr, &clipboard_addr_len)) == -1) {
            perror("main: accept() ");
            continue;
        } else {
            puts("main: Connection accepted");
        }

        //Initialization of clip_thread responsable for apps
        pthread_t app;
        pthread_create(&app,NULL,clip_thread,&client_fd);

    }

    exit(0);
}
