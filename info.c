#include <sys/types.h>
#include <sys/queue.h>
#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>

extern struct evhttp* http_server;

void register_info_json(const char* url);


// { "eventName": "Test Event", "serverTime": 0, "startTime": 5, "Duration": 5 }

static void do_info(struct evhttp_request*, void*);

void register_info_json(const char* url)
{
    evhttp_set_cb(http_server, url, do_info, 0);
}

struct req_data
{
    struct evhttp_request* req;
    struct evkeyvalq* query_args;
    struct evhttp_connection* conn;
    
};

static void free_req_data(struct req_data** _d)
{
    struct req_data* d = *_d;
    *_d = 0;
    
    if(d->query_args)
    {
        struct evkeyval* x;
        while(x = d->query_args->tqh_first)
        {
            TAILQ_REMOVE(d->query_args, x, next);
            free(x->key);
            free(x->value);
            free(x);
        }
        free(d->query_args);
    }
    free(d);
}

static void do_info(struct evhttp_request* req, void* userdata)
{
    // Check request method
    if(req->type == EVHTTP_REQ_POST || req->type == EVHTTP_REQ_HEAD)
    {
        evhttp_send_error(req, 405, "Method Not Allowed");
        return;
    }

    // Allocate userdata structure
    struct req_data* d;
    if(!(d = (struct req_data*) malloc(sizeof(struct req_data))))
    {
        evhttp_send_error(req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): couldn't malloc req_data\n");
        return;
    }

    memset(d, 0, sizeof(struct req_data));
    d->req = req;

    // Parse url query parameters
    char * decoded_uri;
    if(!(decoded_uri = evhttp_decode_uri(req->uri)))
    {
        free_req_data(&d);
        evhttp_send_error(req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): couldn't decode uri\n");
        return;
    }

    if(!(d->query_args = (struct evkeyvalq*) malloc(sizeof(struct evkeyvalq))))
    {
        free_req_data(&d);
        evhttp_send_error(req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): couldn't malloc evkeyvalq\n");
        return;
    }

    memset(d->query_args, 0, sizeof(struct evkeyvalq));
    evhttp_parse_query(decoded_uri, d->query_args);
    free(decoded_uri);

    // find id field
    
    const char* id = evhttp_find_header(d->query_args, "id");

    if(!id)
    {
        free_req_data(&d);
        evhttp_send_error(req, 404, "Not Found");
        return;
    }

    printf("Got id: %s\n", id);

    // return static result

    struct evbuffer *buf;
    buf = evbuffer_new();
    if (buf == NULL)
    {
        free_req_data(&d);
        evhttp_send_error(req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): couldn't alloc evbuffer\n");
        return;
    }

    evhttp_remove_header(req->output_headers, "content-type");
    evhttp_add_header(req->output_headers, "content-type", "application/json");
    evbuffer_add_printf(buf, "{ \"eventName\": \"Test Event\", \"serverTime\": 0, \"startTime\": 5, \"Duration\": 5 }\n");
    evhttp_send_reply(req, 200, "OK", buf);
    evbuffer_free(buf);
    free_req_data(&d);
}


#if 0
/* This is simple demonstration of how to use expat. This program
   reads an XML document from standard input and writes a line with
   the name of each element to standard output indenting child
   elements by one tab stop more than their parent element.
   It must be used with Expat compiled for UTF-8 output.
*/

#include <stdio.h>
#include "expat.h"

#if defined(__amigaos__) && defined(__USE_INLINE__)
#include <proto/expat.h>
#endif

#ifdef XML_LARGE_SIZE
#if defined(XML_USE_MSC_EXTENSIONS) && _MSC_VER < 1400
#define XML_FMT_INT_MOD "I64"
#else
#define XML_FMT_INT_MOD "ll"
#endif
#else
#define XML_FMT_INT_MOD "l"
#endif

static void XMLCALL
startElement(void *userData, const char *name, const char **atts)
{
  int i;
  int *depthPtr = (int *)userData;
  for (i = 0; i < *depthPtr; i++)
    putchar('\t');
  puts(name);
  *depthPtr += 1;
}

static void XMLCALL
endElement(void *userData, const char *name)
{
  int *depthPtr = (int *)userData;
  *depthPtr -= 1;
}

int
main(int argc, char *argv[])
{
  char buf[BUFSIZ];
  XML_Parser parser = XML_ParserCreate(NULL);
  int done;
  int depth = 0;
  XML_SetUserData(parser, &depth);
  XML_SetElementHandler(parser, startElement, endElement);
  do {
    int len = (int)fread(buf, 1, sizeof(buf), stdin);
    done = len < sizeof(buf);
    if (XML_Parse(parser, buf, len, done) == XML_STATUS_ERROR) {
      fprintf(stderr,
              "%s at line %" XML_FMT_INT_MOD "u\n",
              XML_ErrorString(XML_GetErrorCode(parser)),
              XML_GetCurrentLineNumber(parser));
      return 1;
    }
  } while (!done);
  XML_ParserFree(parser);
  return 0;
}

#endif

