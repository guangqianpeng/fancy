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

    int                 sockfd;
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
char *conn_str(connection *conn);

void conn_enable_accept(connection *, event_handler);
void conn_enable_read(connection *, event_handler);
void conn_disable_read(connection *);
void conn_enable_write(connection *, event_handler);
void conn_disable_write(connection *);

int conn_read(connection *conn, buffer *in);
int conn_read_chunked(connection *conn, buffer *in);
int conn_write(connection *conn, buffer *out);
int conn_send_file(connection *conn, int fd, struct stat *st);

#define CONN_READ(conn, in, error_handler) \
do {    \
    int err = conn_read(conn, in); \
    switch(err) {    \
        case FCY_AGAIN: \
            return; \
        case FCY_ERROR: \
            error_handler; \
            return; \
        default:    \
            break;  \
    }   \
} while(0)  \

#define CONN_WRITE(conn, out, error_handler) \
do {    \
    int err = conn_write(conn, out); \
    switch(err) {    \
        case FCY_AGAIN: \
            return; \
        case FCY_ERROR: \
            error_handler; \
            return; \
        default:    \
            break;  \
    }   \
} while(0)  \

#define CONN_SEND_FILE(conn, fd, st, error_handler)   \
do {    \
    int err = conn_send_file(conn, fd, st); \
    switch(err) {    \
        case FCY_AGAIN: \
            return; \
        case FCY_ERROR: \
            error_handler; \
            return; \
        default:    \
            if (st->st_size > 0) {  \
                return; \
            }  \
    }   \
} while(0)  \

#endif //FANCY_CONN_POOL_H
