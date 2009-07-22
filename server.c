#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void do_generic_url(struct evhttp_request*, void*);

struct evhttp* http_server;

int main(int argc, char** argv)
{
    event_init();

    http_server = evhttp_new(0);
    evhttp_bind_socket(http_server, "0.0.0.0", 8080);
    evhttp_set_gencb(http_server, do_generic_url, 0);

    event_dispatch();
}

void do_generic_url(struct evhttp_request* req, void* userdata)
{
    struct evbuffer *buf;
    buf = evbuffer_new();
    if (buf == NULL)
    {
        evhttp_send_error(req, 500, "Internal Server Error");
        return;
    }
    switch(req->type)
    {
        case EVHTTP_REQ_GET:
            printf("GET %s\n", req->uri);
            break;
        case EVHTTP_REQ_POST:
            printf("POST %s\n", req->uri);
            break;
        case EVHTTP_REQ_HEAD:
            printf("HEAD %s\n", req->uri);
            break;
    }
    evbuffer_add_printf(buf, "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>There is nothing to see here</p></body></html>");
    evhttp_send_reply(req, 404, "Not Found", buf);
}
