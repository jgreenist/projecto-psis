all: clipboard app_teste

clipboard: clipboard.c queue.c
	gcc -pthread clipboard.c queue.c -o clipboard

app_teste: app_teste.c library.c
	gcc app_teste.c library.c -o app_teste
