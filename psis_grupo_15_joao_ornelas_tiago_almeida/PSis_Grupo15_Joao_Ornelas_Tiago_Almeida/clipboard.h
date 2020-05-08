
#include <sys/types.h>
#define SOCKADDR "./CLIPBOARD_SOCKET"

typedef struct msg_struct{
    int action;
    int region;
    int size;
} message;

#define COPY 1
#define PASTE 2
#define CLOSE 3
#define WAIT 4



int clipboard_connect(char * clipboard_dir);
int clipboard_copy(int clipboard_id, int region, void *buf, size_t count);
int clipboard_paste(int clipboard_id, int region, void *buf, size_t count);
void clipboard_close(int clipboard_id);
int clipboard_wait(int clipboard_id,int region, void*buf,size_t count);

