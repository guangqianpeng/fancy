//
// Created by frank on 17-5-16.
//

#ifndef FANCY_UPSTREAM_H
#define FANCY_UPSTREAM_H

#include "base.h"
#include "http_parser.h"
#include "connection.h"
#include "chunk_reader.h"
#include "request.h"

typedef struct upstream upstream;

struct upstream {

    unsigned    has_content_length_header:1;
    unsigned    has_server_header:1;
    unsigned    is_chunked:1;

    /* 读header时可能会读一部分body进来
     * 这样第一次调用readbody时不必读
     * */
    unsigned    avoid_read_body;

    long        content_length;

    buffer      *header_in;
    buffer      *header_out;
    buffer      *body_in;
    buffer      *body_out;

    string     server;
    string     connection;
    array       *headers;

    http_parser     parser;
    chunk_reader    reader;
};

upstream *upstream_create(peer_connection *, mem_pool *);
void upstream_destroy(upstream *);
int upstream_parse(upstream *);
void upstream_headers_htop(upstream *, buffer *);
int upstream_read_chunked(upstream *);

#endif //FANCY_UPSTREAM_H
