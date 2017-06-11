//
// Created by frank on 17-5-18.
//

#include "log.h"
#include "upstream.h"
#include "request.h"
#include "chunk_reader.h"

static void upstream_set_parser(upstream *u);
static void upstream_on_header(void *user, string *name, string *value);

upstream *upstream_create(peer_connection *conn, mem_pool *p)
{
    upstream *u;

    u = pcalloc(p, sizeof(upstream));
    if (u == NULL) {
        return NULL;
    }

    u->headers = array_create(p, 10, sizeof(keyval));
    if (u->headers == NULL) {
        return NULL;
    }

    u->header_in = buffer_create(p, HTTP_BUFFER_SIZE);
    u->header_out = buffer_create(p, HTTP_BUFFER_SIZE);
    u->body_in = buffer_create(p, HTTP_BUFFER_SIZE);
    u->body_out = buffer_create(p, HTTP_BUFFER_SIZE);
    if (u->header_in == NULL
        || u->header_out == NULL
        || u->body_in == NULL
        || u->body_out == NULL) {
        return NULL;
    }

    upstream_set_parser(u);

    conn->app = u;

    return u;
}

void upstream_destroy(upstream *u)
{
    buffer_destroy(u->body_out);
    buffer_destroy(u->body_in);
    buffer_destroy(u->header_out);
    buffer_destroy(u->header_in);
    array_destroy(u->headers);
}

int upstream_parse(upstream *u)
{
    char *beg = buffer_peek(u->header_in);
    char *end = buffer_begin_write(u->header_in);
    return parser_execute(&u->parser, beg, end);
}

void upstream_headers_htop(upstream *u, buffer *b)
{
    assert(buffer_empty(b));

    http_parser *p = &u->parser;

    /* line */
    buffer_append_str(b, &p->response_line);

    /* headers */
    buffer_append_literal(b, "\r\nServer: fancy beta");
    buffer_append_literal(b, "\r\nConnection: close\r\n");

    for (size_t i = 0; i < u->headers->size; ++i) {
        keyval *kv = array_at(u->headers, i);
        buffer_append_str(b, &kv->key);
        buffer_append_literal(b, ": ");
        buffer_append_str(b, &kv->value);
        buffer_append_crlf(b);
    }
    buffer_append_crlf(b);
}

int upstream_read_chunked(upstream *u)
{
    assert(u->is_chunked);

    char *beg = buffer_peek(u->body_in);
    char *end = buffer_begin_write(u->body_in);

    return chunk_reader_execute(&u->reader, beg, end);
}

static void upstream_set_parser(upstream *u)
{
    u->parser.type = HTTP_PARSE_RESPONSE;
    u->parser.header_cb = upstream_on_header;
    u->parser.user = u;
}

static void upstream_on_header(void *user, string *name, string *value)
{
    upstream *u = user;
    if (strcasecmp(name->data, "Content-Length") == 0) {
        u->has_content_length_header = 1;
        u->content_length = atoi(value->data);
    }
    else if (strcasecmp(name->data, "Connection") == 0) {
        u->connection = *value;
        return;
    }
    else if (strcasecmp(name->data, "Server") == 0) {
        u->server = *value;
        return;
    }
    else if (strcasecmp(name->data, "Transfer-Encoding") == 0) {
        if (strcasecmp(value->data, "chunked") == 0) {
            u->is_chunked = 1;
        }
    }

    keyval *kv = array_alloc(u->headers);
    if (kv == NULL) {
        LOG_FATAL("array_alloc error, run out of memory");
    }
    kv->key = *name;
    kv->value = *value;
}