//
// Created by frank on 17-5-16.
//

#ifndef FANCY_UPSTREAM_H
#define FANCY_UPSTREAM_H

#include "base.h"
#include "request.h"
#include "../event/connection.h"

typedef struct upstream upstream;

struct upstream {

    request     *rqst;
    connection  *conn;
    buffer      *request;   /* 用于传给上游的 header & body */
    buffer      *response;  /* 用于传给下游的 header & body */
};

upstream *upstream_create(peer_connection *);
void upstream_destroy(upstream *);


#endif //FANCY_UPSTREAM_H
