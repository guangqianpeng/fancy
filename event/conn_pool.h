//
// Created by frank on 17-2-12.
//

#ifndef FANCY_CONN_POOL_H
#define FANCY_CONN_POOL_H

#include "event.h"

int conn_pool_init(mem_pool *p, int size);
connection *conn_pool_get();
void conn_pool_free(connection *conn);

#endif //FANCY_CONN_POOL_H
