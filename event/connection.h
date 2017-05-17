//
// Created by frank on 17-2-12.
//

#ifndef FANCY_CONN_POOL_H
#define FANCY_CONN_POOL_H

#include "event.h"

typedef struct connection connection;
typedef struct connection peer_connection;

/* call conn_get to get a connection */
struct connection {

    int                 fd;
    event               read;
    event               write;

    peer_connection     *peer;  // 上游/下游连接

    void                *app;   // request, upstream
    int                 app_count;

    struct sockaddr_in  addr;

    list_node           node;
};

int conn_pool_init(mem_pool *p, int size);
connection *conn_get();
void conn_free(connection *conn);

int conn_enable_read(connection *conn, event_handler handler, uint32_t epoll_flag);
int conn_disable_read(connection *conn, uint32_t epoll_flag);
int conn_enable_write(connection *conn, event_handler handler, uint32_t epoll_flag);
int conn_disable_write(connection *conn, uint32_t epoll_flag);

#endif //FANCY_CONN_POOL_H
