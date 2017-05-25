//
// Created by frank on 17-5-16.
//

#ifndef FANCY_UPSTREAM_H
#define FANCY_UPSTREAM_H

#include "base.h"
#include "http_parser.h"
#include "connection.h"

typedef struct upstream upstream;

struct upstream {
    long        content_length;
    http_parser parser;
};

upstream *upstream_create(peer_connection *, mem_pool *);
int upstream_parse(upstream *, buffer *in);
void upstream_destroy(upstream *);

#endif //FANCY_UPSTREAM_H
