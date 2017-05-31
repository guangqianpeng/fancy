//
// Created by frank on 17-5-18.
//

#include "upstream.h"

static void upstream_set_parser(upstream *r);
static void upstream_on_header(void *user, char *name, char *value);

upstream *upstream_create(peer_connection *conn, mem_pool *p)
{
    upstream *u;

    u = pcalloc(p, sizeof(upstream));
    if (u == NULL) {
        return NULL;
    }

    upstream_set_parser(u);

    conn->app = u;

    return u;
}

void upstream_destroy(upstream *u)
{

}

int upstream_parse(upstream *u, buffer *in)
{
    return parser_execute(&u->parser, in);
}

static void upstream_set_parser(upstream *r)
{
    r->parser.type = HTTP_PARSE_RESPONSE;
    r->parser.header_cb = upstream_on_header;
    r->parser.user = r;
}

static void upstream_on_header(void *user, char *name, char *value)
{
    upstream *u = user;
    if (strncasecmp(name, "Content-Length", 14) == 0) {
        u->content_length = strtol(value, NULL, 0);
    }
}