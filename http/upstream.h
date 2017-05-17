//
// Created by frank on 17-5-16.
//

#ifndef FANCY_UPSTREAM_H
#define FANCY_UPSTREAM_H

#include "base.h"
#include "request.h"
#include "../event/event.h"

typedef struct upstream upstream;

struct upstream {

    request     *rqst;
    connection  *conn;
    buffer      *request;   /* 用于传给上游的 header & body */
    buffer      *response;  /* 用于传给下游的 header & body */
};

#endif //FANCY_UPSTREAM_H
