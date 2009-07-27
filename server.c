#include <sys/types.h>
#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct evhttp* http_server;

void register_static_url(const char* url, const char* content_type, const char* file);
void register_static_url_gz(const char* url, const char* content_type, const char* file, const char* gzfile);
void register_info_json(const char* url);
void register_during(const char* url);
void register_after(const char* url);
void register_select_url(const char* url);

void do_generic_url(struct evhttp_request*, void*);
void do_show_event(struct evhttp_request*, void*);

int main(int argc, char** argv)
{
    setenv("TZ", "", 1);

    event_init();

    http_server = evhttp_new(0);
    evhttp_bind_socket(http_server, "0.0.0.0", 8080);
    evhttp_set_gencb(http_server, do_generic_url, 0);
    evhttp_set_cb(http_server, "/show", do_show_event, 0);
    register_static_url("/eventbase.css", "text/css", "eventbase.css");
    register_static_url("/embed", "text/html", "embed.html");
    register_static_url("/before", "text/html", "before.html");
    register_static_url("/", "text/html", "index.html");
    register_during("/during");
    register_after("/after");
    register_info_json("/info");
    register_selector_url("/select");
    register_static_url_gz("/jquery.js", "application/javascript", "jquery-1.3.2.min.js", "jquery-1.3.2.min.js.gz");
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
    evbuffer_free(buf);
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
    evbuffer_free(buf);
}
