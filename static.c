#include <sys/types.h>
#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

static void return_static_content(struct evhttp_request*, void*);
static void return_gz_content(struct evhttp_request*, void*);

extern struct evhttp* http_server;

void register_static_url(const char* url, const char* content_type, const char* file);
void register_static_url_gz(const char* url, const char* content_type, const char* file, const char* gzfile);

struct static_content
{
    size_t len;
    void * data;
    const char * content_type;
};

static struct static_content* read_file(const char* content_type, const char* file)
{
    int fd;
    struct stat fileinfo;
    struct static_content* c;

    c = (struct static_content*) malloc(sizeof(struct static_content));
    if(!c) return 0;
    
    c->content_type = content_type;
    fd = open(file, O_RDONLY);

    fstat(fd, &fileinfo);
    c->len = fileinfo.st_size;
    c->data = mmap(0, c->len, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return c;
}

void register_static_url(const char* url, const char* content_type, const char* file)
{
    evhttp_set_cb(http_server, url, return_static_content, read_file(content_type, file));
}

static void return_500(struct evhttp_request* req, void* content)
{
    evhttp_send_error(req, 500, (const char*) content);
}

static void return_static_content(struct evhttp_request* req, void* _content)
{
    if(!_content)
    {
        evhttp_send_error(req, 500, "Internal Server Error");
        return;
    }

    if(req->type == EVHTTP_REQ_POST)
    {
        printf("POST %s\n", req->uri);
        evhttp_send_error(req, 405, "Method Not Allowed");
        return;
    }

    struct static_content* content = (struct static_content*) _content;
    char tmpstr[64];
    sprintf(tmpstr, "%d", (int) content->len);
    evhttp_remove_header(req->output_headers, "content-type");
    evhttp_add_header(req->output_headers, "content-type", content->content_type);
    evhttp_remove_header(req->output_headers, "content-length");
    evhttp_add_header(req->output_headers, "content-length", tmpstr);
    if(req->type == EVHTTP_REQ_HEAD)
    {
        printf("HEAD %s\n", req->uri);
        evhttp_send_error(req, 200, "OK");
        return;
    }
    
    printf("GET %s\n", req->uri);
    
    struct evbuffer *buf;
    buf = evbuffer_new();
    evbuffer_add(buf, content->data, content->len);

    evhttp_send_reply(req, 200, "OK", buf);
    evbuffer_free(buf);
}

void register_static_url_gz(const char* url, const char* content_type, const char* file, const char* gzfile)
{
    struct static_content** c;

    c = (struct static_content**) malloc(2*sizeof(struct static_content*));

    if(!c)
    {
        evhttp_set_cb(http_server, url, return_500, strerror(errno));
    }

    c[0] = read_file(content_type, file);
    c[1] = read_file(content_type, gzfile);

    evhttp_set_cb(http_server, url, return_gz_content, c);
}

void return_gz_content(struct evhttp_request* req, void* _content)
{
    if(!_content)
    {
        evhttp_send_error(req, 500, "Internal Server Error");
        return;
    }

    struct static_content** c = (struct static_content**) _content;

    if(!c[0] || !c[1])
    {
        evhttp_send_error(req, 500, "Internal Server Error");
        return;
    }

    if(req->type == EVHTTP_REQ_POST)
    {
        evhttp_send_error(req, 405, "Method Not Allowed");
        return;
    }

    struct static_content* content = c[0];

    const char* accept_encoding = evhttp_find_header(req->input_headers, "accept-encoding");

    if(accept_encoding && strstr(accept_encoding, "gzip"))
    {
        evhttp_add_header(req->output_headers, "content-encoding", "gzip");
        content = c[1];
    }

    char tmpstr[64];
    sprintf(tmpstr, "%d", (int) content->len);
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
    evbuffer_free(buf);
}
