#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

void do_generic_url(struct evhttp_request*, void*);
void do_show_event(struct evhttp_request*, void*);
void return_static_content(struct evhttp_request*, void*);

struct evhttp* http_server;

void register_static_url(const char* url, const char* file, const char* content_type);

struct static_content
{
    size_t len;
    void * data;
    const char * content_type;
};

int main(int argc, char** argv)
{
    event_init();

    http_server = evhttp_new(0);
    evhttp_bind_socket(http_server, "0.0.0.0", 8080);
    evhttp_set_gencb(http_server, do_generic_url, 0);
    evhttp_set_cb(http_server, "/show", do_show_event, 0);
    register_static_url("/hello", "hello.html", "text/html");

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

void return_500(struct evhttp_request* req, void* _reason)
{
    evhttp_send_error(req, 500, (char *) _reason);
}

void register_static_url(const char* url, const char* file, const char* content_type)
{
    int fd;
    struct stat fileinfo;
    struct static_content* c;

    c = (struct static_content*) malloc(sizeof(struct static_content));

    c->content_type = content_type;
    
    fd = open(file, O_RDONLY);

    fstat(fd, &fileinfo);
    c->len = fileinfo.st_size;
    c->data = mmap(0, c->len, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    evhttp_set_cb(http_server, url, return_static_content, c);
}

void return_static_content(struct evhttp_request* req, void* _content)
{
    if(req->type == EVHTTP_REQ_POST)
    {
        evhttp_send_error(req, 405, "Method Not Allowed");
        return;
    }

    struct static_content* content = (struct static_content*) _content;
    char tmpstr[64];
    sprintf(tmpstr, "%d", content->len);
    evhttp_remove_header(req->output_headers, "content-type");
    evhttp_add_header(req->output_headers, "content-type", content->content_type);
    evhttp_remove_header(req->output_headers, "content-length");
    evhttp_add_header(req->output_headers, "content-length", tmpstr);
    if(req->type == EVHTTP_REQ_HEAD)
    {
        evhttp_send_error(req, 200, "OK");
        return;
    }
    
    struct evbuffer *buf;
    buf = evbuffer_new();
    evbuffer_add(buf, content->data, content->len);

    evhttp_send_reply(req, 200, "OK", buf);
}

void do_show_event(struct evhttp_request* req, void* userdata)
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
    evhttp_remove_header(req->output_headers, "content-type");
    evhttp_add_header(req->output_headers, "content-type", "text/plain");
    evbuffer_add_printf(buf, "<html><head><title>200 OK</title></head><body><h1>200 OK</h1><p>You made it!</p></body></html>");
    evhttp_send_reply(req, 200, "OK", buf);
}
