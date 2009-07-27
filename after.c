#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/queue.h>
#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <expat.h>
#include <time.h>
#include <regex.h>
#include <string.h>

extern struct evhttp* http_server;

void register_after(const char* url);


// { "eventName": "Test Event", "serverTime": 0, "startTime": 5, "Duration": 5 }

static void do_info(struct evhttp_request*, void*);
static void got_api(struct evhttp_request* req, void* userdata);

static void XMLCALL startElement(void *userData, const char *name, const char **atts);
static void XMLCALL endElement(void *userData, const char *name);
static void XMLCALL xmlData(void *userData, const XML_Char *s, int len);

static char* escape_json_string(const char* in);

void register_after(const char* url)
{
    evhttp_set_cb(http_server, url, do_info, 0);
}

struct req_data
{
    struct evhttp_request* req;
    struct evkeyvalq* query_args;
    struct evhttp_connection* conn;
    time_t start;
    const char* login;
    int offset;
    int count;
    int done;

    char* best_embed_code;
    time_t best_start;
    
    char* cur_embed_code;
    time_t cur_start;

    char xmlData[4096];
    int xmlDataOff;
};

static void do_api_request(struct req_data* d);

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

    if(d->conn) evhttp_connection_free(d->conn);
    if(d->best_embed_code) free(d->best_embed_code);
    if(d->cur_embed_code) free(d->cur_embed_code);
    
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

    // find id field; build request
    
    const char* login = evhttp_find_header(d->query_args, "channel");

    if(!login)
    {
        free_req_data(&d);
        evhttp_send_error(req, 404, "Not Found");
        return;
    }

    d->login = login;

    const char* start_time = evhttp_find_header(d->query_args, "start");
    if(!start_time)
    {
        free_req_data(&d);
        evhttp_send_error(req, 404, "Not Found");
        return;
    }
    
    d->start = (time_t) atol(start_time);
    
    do_api_request(d);
}

static void do_api_request(struct req_data* d)
{
    struct evhttp_request* api_req;

    if(d->conn) evhttp_connection_free(d->conn);
    d->conn = 0;

    if(!(api_req = evhttp_request_new(got_api, d)))
    {
        free_req_data(&d);
        evhttp_send_error(d->req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): couldn't alloc evhttp_request");
        return;
    }
    
    char api_url[64];
    sprintf(api_url, "/api/channel/clips/%s.xml?limit=100&offset=%d", d->login, d->offset);
    d->offset += 100;

    evhttp_add_header(api_req->output_headers, "host", "staging.justin.tv");
    printf("%s\n", api_url);
    // issue request

    if(!(d->conn = evhttp_connection_new("staging.justin.tv", 80)))
    {
        evhttp_request_free(api_req);
        free_req_data(&d);
        evhttp_send_error(d->req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): couldn't alloc evhttp_connection\n");
        return;
    }

    if(evhttp_make_request(d->conn, api_req, EVHTTP_REQ_GET, api_url) == -1)
    {
        free_req_data(&d);
        evhttp_send_error(d->req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_info(): error making api request\n");
    }
}

static void got_api(struct evhttp_request* req, void* userdata)
{
    struct req_data* d = (struct req_data*) userdata;

    if(req->response_code != 200)
    {
        evhttp_send_error(d->req, 500, "Internal Server Error");
        free_req_data(&d);
        fprintf(stderr, "info.c:got_api(): api returned http status %d\n", (int)(req->response_code));
        return;
    }

    XML_Parser parser = XML_ParserCreate(NULL);
    int done;
    int depth = 0;
    XML_SetUserData(parser, d);
    XML_SetElementHandler(parser, startElement, endElement);
    XML_SetCharacterDataHandler(parser, xmlData);

    d->count = 0;

    char buf[4096];
    int buflen;
    while((buflen = evbuffer_remove(req->input_buffer, buf, 4096)) > 0)
    {
        if(XML_Parse(parser, buf, buflen, 0) == XML_STATUS_ERROR)
        {
            evhttp_send_error(d->req, 500, "Internal Server Error");
            free_req_data(&d);
            fprintf(stderr, "info.c:got_api(): error parsing xml: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
            return;
        }
    }
    XML_ParserFree(parser);

    // return static result

    if((! d->done) && (d->count == 100))
    {
        do_api_request(d);
        return;
    }

    struct evbuffer *evbuf;
    evbuf = evbuffer_new();
    if (buf == NULL)
    {
        evhttp_send_error(d->req, 500, "Internal Server Error");
        free_req_data(&d);
        fprintf(stderr, "info.c:got_api(): couldn't alloc evbuffer\n");
        return;
    }

    evhttp_remove_header(d->req->output_headers, "content-type");
    evhttp_add_header(d->req->output_headers, "content-type", "text/html");

    regex_t width_re;
    regex_t height_re;

    regmatch_t match;
    char s1[5192];
    char s2[5192];
    char* p;

    int result;

    regcomp(&width_re, "width=\"[0-9]*\"", REG_EXTENDED);
    regcomp(&height_re, "height=\"[0-9]*\"", REG_EXTENDED);

    result = regexec(&width_re, d->best_embed_code, 1, &match, 0);

    if(!result)
    {
        d->best_embed_code[match.rm_so] = '\0';
        p = stpcpy(s1, d->best_embed_code);
        p = stpcpy(p, "width=\"100%\"");
        stpcpy(p, d->best_embed_code+match.rm_eo);
    }
    else
    {
        strcpy(s1, d->best_embed_code);
    }
   
    result = regexec(&height_re, s1, 1, &match, 0);

    if(!result)
    {
        s1[match.rm_so] = '\0';
        p = stpcpy(s2, s1);
        p = stpcpy(p, "height=\"100%\"");
        stpcpy(p, s1+match.rm_eo);
    }
    else
    {
        strcpy(s2, s1);
    }

    regfree(&width_re);
    regfree(&height_re);

    evbuffer_add_printf(evbuf, "%s", s2);
    evhttp_send_reply(d->req, 200, "OK", evbuf);
    evbuffer_free(evbuf);
    free_req_data(&d);
}

static void XMLCALL startElement(void *userData, const char *name, const char **atts)
{
    struct req_data* d = (struct req_data*) userData;
    d->xmlDataOff = 0;
}

static void XMLCALL endElement(void *userData, const char *name)
{
    struct req_data* d = (struct req_data*) userData;
    if(d->done) return;

    d->xmlData[d->xmlDataOff] = '\0';
    if(!strcmp(name, "clip"))
    {
        long best_diff = abs(d->best_start - d->start);
        long cur_diff = abs(d->cur_start - d->start);
        if( cur_diff > best_diff ) { d->done = 1; return; }
        free(d->best_embed_code);
        d->best_embed_code = d->cur_embed_code;
        d->best_start = d->cur_start;
        d->cur_embed_code = 0;
        d->cur_start = 0;
    }
    else if(!strcmp(name, "embed_code"))
    {
        d->cur_embed_code = (char *) malloc(d->xmlDataOff+2);
        strcpy(d->cur_embed_code, d->xmlData);
    }
    else if(!strcmp(name, "start_time"))
    {
        struct tm date;
        memset(&date, 0, sizeof(struct tm));
        strptime(d->xmlData, "%Y-%m-%dT%H:%M:%SZ", &date);
        d->cur_start = mktime(&date);
    }
    d->xmlDataOff = 0;   
}

static void XMLCALL xmlData(void *userData, const XML_Char *s, int len)
{
    struct req_data* d = (struct req_data*) userData;

    if(len+d->xmlDataOff > 4095) len = 4095-d->xmlDataOff;

    memcpy(d->xmlData+d->xmlDataOff,s, len);
    d->xmlDataOff += len;
}

static char* escape_json_string(const char* in)
{
    int finalSize = 0;
    char* rtn;
    const char* tmpi = in;
    while(*tmpi)
    {
        if(*tmpi == '"' || *tmpi == '\\') finalSize++;
        finalSize++;
        tmpi++;
    }

    rtn = (char*) malloc(finalSize+2);

    if(rtn == 0) return 0;

    char* tmpo = rtn;
    tmpi = in;

    while(*tmpi)
    {
        if(*tmpi == '"' || *tmpi == '\\') { *tmpo = '\\'; tmpo++; } 
        *tmpo = *tmpi;
        tmpi++; tmpo++;
    }
    *tmpo = '\0';

    return rtn;
}

