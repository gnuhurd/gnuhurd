#include <sys/types.h>
#include <sys/queue.h>
#include <event.h>
#include <evhttp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct evhttp* http_server;

void register_during(const char* url);

// { "eventName": "Test Event", "serverTime": 0, "startTime": 5, "Duration": 5 }

static void do_during(struct evhttp_request*, void*);

void register_during(const char* url)
{
    evhttp_set_cb(http_server, url, do_during, 0);
}

static void do_during(struct evhttp_request* req, void* userdata)
{
    // Check request method
    if(req->type == EVHTTP_REQ_POST || req->type == EVHTTP_REQ_HEAD)
    {
        evhttp_send_error(req, 405, "Method Not Allowed");
        return;
    }

    // Parse url query parameters
    char * decoded_uri;
    if(!(decoded_uri = evhttp_decode_uri(req->uri)))
    {
        evhttp_send_error(req, 500, "Internal Server Error");
        fprintf(stderr, "info.c:do_during(): couldn't decode uri\n");
        return;
    }

    struct evkeyvalq args;
    struct evkeyval* x;

    memset(&args, 0, sizeof(struct evkeyvalq));
    evhttp_parse_query(decoded_uri, &args);
    free(decoded_uri);

    const char* login = evhttp_find_header(&args, "channel");

    if(!login)
    {
        while(x = args.tqh_first)
        {
            TAILQ_REMOVE(&args, x, next);
            free(x->key);
            free(x->value);
            free(x);
        }

        evhttp_send_error(req, 404, "Not Found");
        return;
    }

    struct evbuffer* out;
    out = evbuffer_new();
    evbuffer_add_printf(out, "<object type=\"application/x-shockwave-flash\" height=\"100%%\" width=\"100%%\" id=\"live_embed_player_flash\" data=\"http://www.justin.tv/widgets/live_embed_player.swf?channel=%s\" bgcolor=\"#000000\"><param name=\"allowFullScreen\" value=\"true\" /><param name=\"allowscriptaccess\" value=\"always\" /><param name=\"movie\" value=\"http://www.justin.tv/widgets/live_embed_player.swf\" /><param name=\"flashvars\" value=\"channel=%s&auto_play=true&start_volume=50\" /></object>", login, login);

    while(x = args.tqh_first)
    {
        TAILQ_REMOVE(&args, x, next);
        free(x->key);
        free(x->value);
        free(x);
    }

    evhttp_add_header(req->output_headers, "content-type", "text/html");
    evhttp_send_reply(req, 200, "OK", out);
    evbuffer_free(out);

}
