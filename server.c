#include <event.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

void do_accept(int, short, void*);
void do_read(int, short, void*);

struct event ev_in;

int main(int argc, char** argv)
{
    // Common initialization (accept socket)

    struct sockaddr_in listen_addr;

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_port = htons(8080);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    bind(listener, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
    listen(listener, 32);

    // fork(); fork(); fork(); // To use all the cpu cores

    int result;

    event_init();
    event_set(&ev_in, listener, EV_READ | EV_PERSIST, do_accept, NULL);
    result = event_add(&ev_in, NULL);
    result = event_dispatch();
}

void do_accept(int fd, short ev_type, void* data)
{
    int newfd;
    struct event * ev_read;
    switch(ev_type)
    {
        case EV_READ:
            newfd = accept(fd, 0, 0);
            ev_read = (struct event *) malloc(sizeof(struct event));
            if(!ev_read) { close(newfd); return; }
            event_set(ev_read, newfd, EV_READ | EV_PERSIST, do_read, ev_read);
            event_add(ev_read, NULL);
            break;
    }
}

void do_read(int fd, short ev_type, void* ev_struct)
{
    char buf[1024];
    int count;

    switch(ev_type)
    {
        case EV_READ:            
            count = read(fd, buf, 1024);
            if(count > 0) write(1, buf, count);
            else if(count == 0)
            {
                event_del((struct event *)ev_struct);
                free(ev_struct);
                close(fd);
            }
            break;
    }
}
