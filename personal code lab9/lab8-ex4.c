#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "lab8-ex4.h"
//gcc teste.c UI_library.c -o teste-UI -lSDL2 -lSDL2_image

#include "UI_library.h"
Uint32 Event_ShowCharacter;
int sock_fd;

void * clientThread(void * arg){
	exe4_message msg;
	int err_rcv;
	exe4_message *event_data;
	SDL_Event new_event;
	printf("just connected to the server\n");
		//loop receinving messages from socket

	while((err_rcv = recv(sock_fd, &msg , sizeof(msg), 0)) >0 ){
    printf("received %d byte %d %d %d\n", err_rcv, msg.character, msg.x, msg.y);
		event_data = malloc(sizeof(exe4_message));
		*event_data = msg;
		SDL_zero(new_event);
		new_event.type = Event_ShowCharacter;
		new_event.user.data1 = event_data;
		SDL_PushEvent(&new_event);
	}
	return (NULL);
}

int server_socket;
void * serverThread(void * argc){
	exe4_message msg;
	exe4_message *event_data;
	struct sockaddr_in client_addr;
	int err_rcv;
	socklen_t size_addr = sizeof(client_addr);
	SDL_Event new_event;

	while(1){
		printf("waiting for connections\n");
    sock_fd= accept(server_socket,
                        (struct sockaddr *) & client_addr, &size_addr);
    if(sock_fd == -1) {
      perror("accept");
      exit(-1);
    }
    printf("accepted connection \n");

		while((err_rcv = recv(sock_fd, &msg , sizeof(msg), 0)) >0 ){
	    printf("received %d byte %d %d %d\n", err_rcv, msg.character, msg.x, msg.y);

				event_data = malloc(sizeof(exe4_message));
				*event_data = msg;
				SDL_zero(new_event);
				new_event.type = Event_ShowCharacter;
				new_event.user.data1 = event_data;
				SDL_PushEvent(&new_event);
		}

	}
	return (NULL);
}


int main(int argc , char* argv[]){

	int is_server;
	SDL_Event event;
	int done = 0;
	pthread_t thread_id;

	Event_ShowCharacter =  SDL_RegisterEvents(1);
	if (argc == 1){
		struct sockaddr_in server_local_addr;

	 //it is a server
	 is_server = 1;


	 server_socket = socket(AF_INET, SOCK_STREAM, 0);
	 if (server_socket == -1){
      perror("socket: ");
      exit(-1);
    }
		server_local_addr.sin_family = AF_INET;
    server_local_addr.sin_addr.s_addr = INADDR_ANY;
    server_local_addr.sin_port = htons(3000);
    int err = bind(server_socket, (struct sockaddr *)&server_local_addr,
		    									sizeof(server_local_addr));
    if(err == -1) {
      perror("bind");
      exit(-1);
    }
		if(listen(server_socket, 5) == -1) {
      perror("listen");
      exit(-1);
    }
		// can we do the accept here?
		// no the accept should be on the thread
	 pthread_create(&thread_id, NULL, serverThread, NULL);
	}
	if(argc == 3){

	 // it is a client
		struct sockaddr_in server_addr;

		sock_fd= socket(AF_INET, SOCK_STREAM, 0);
		if (sock_fd == -1){
		  perror("socket: ");
		  exit(-1);
	  }

		server_addr.sin_family = AF_INET;
		int port_number;
		if(sscanf(argv[2], "%d", &port_number)!=1){
			printf("argv[2] is not a number\n");
			exit(-1);
		}
	  server_addr.sin_port= htons(port_number);
	  if(inet_aton(argv[1], &server_addr.sin_addr) == 0){
			printf("argv[1]is not a valida address\n");
			exit(-1);
		}

	  printf("connecting to %s %d\n", argv[1], server_addr.sin_port );

		if( -1 == connect(sock_fd,
	  			        (const struct sockaddr *) &server_addr,	sizeof(server_addr))){
	  				printf("Error connecting\n");
	  				exit(-1);
	  	}

		 is_server = 0;
		 pthread_create(&thread_id, NULL, clientThread, NULL);
	}
	if((argc!= 1) && (argc != 3)){
	 exit(-1);
	}
	//creates a windows and a board with 50x20 cases
	create_board_window(50, 20);

	//monster and packman position
	int x = 0;
	int y = 0;

	int x_other = 0;
	int y_other = 0;

	while (!done){
		while (SDL_PollEvent(&event)) {
			if(event.type == SDL_QUIT) {
					done = SDL_TRUE;
			}
			if(event.type == Event_ShowCharacter){
				exe4_message * data_ptr;
				data_ptr = event.user.data1;

				clear_place(x_other, y_other);
				x_other = data_ptr->x;
				y_other = data_ptr->y;
				if(is_server){
					paint_monster(x_other, y_other , 7, 200, 100);
				}else{
					paint_pacman(x_other, y_other , 7, 100, 200);
				}
				printf("new event received\n");
			}

			//when the mouse mooves the monster also moves
			if(event.type == SDL_MOUSEMOTION){
				int x_new, y_new;

				//this fucntion return the place cwher the mouse cursor is
				get_board_place(event.motion .x, event.motion .y,
												&x_new, &y_new);
				//if the mluse moved toi anothe place
				if((x_new != x) || (y_new != y)){
					//the old place is cleared
					clear_place(x, y);
					x = x_new;
					y=y_new;
					//decide what color to paint the monster
					//and paint it
					exe4_message msg;
					if(is_server){
						paint_pacman(x, y , 200, 100, 7);
						msg.character = 2;
					}else{
						paint_monster(x, y , 7, 200, 100);
						msg.character = 3;
					}
					printf("move x-%d y-%d\n", x,y);
					msg.x = x;
					msg.y = y;
					send(sock_fd, &msg, sizeof(msg), 0);
				}
			}
		}
	}

	printf("fim\n");
	close_board_windows();
	exit(0);
}
