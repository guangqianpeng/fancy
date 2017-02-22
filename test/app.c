//
// Created by frank on 17-2-19.
//

#include "app.h"
#include "conn_pool.h"
#include "timer.h"

static int tcp_listen(uint16_t serv_port);

int init_server(int n_conn, int n_event)
{
    mem_pool    *pool;
    size_t      size;

    size = n_conn * sizeof (connection) + n_conn * sizeof (connection) + sizeof(mem_pool);
    pool = mem_pool_create(size);

    if (pool == NULL){
        return FCY_ERROR;
    }

    if (conn_pool_init(pool, n_conn) == -1) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (event_init(pool, n_event) == -1) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    timer_init();

    return FCY_OK;
}

int add_accept_event(uint16_t serv_port, event_handler accept_handler)
{
    int         listenfd;
    connection  *conn;

    listenfd = tcp_listen(serv_port);

    conn = conn_pool_get();
    if (conn == NULL) {
        return FCY_ERROR;
    }

    conn->fd = listenfd;
    conn->read->handler = accept_handler;

    if (event_add(conn->read) == FCY_ERROR) {
        return FCY_ERROR;
    }

    return FCY_OK;
}

static int tcp_listen(uint16_t serv_port)
{
    int                 listenfd;
    struct sockaddr_in  servaddr;
    socklen_t           addrlen;
    const int           sockopt;
    int                 err;

    listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenfd == -1) {
        err_sys("socket error");
    }

    err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    if (err == -1) {
        err_sys("setsockopt error");
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    servaddr.sin_port           = htons(serv_port);

    addrlen = sizeof(servaddr);
    err = bind(listenfd, (struct sockaddr*)&servaddr, addrlen);
    if (err == -1) {
        err_sys("bind error");
    }

    err = listen(listenfd, 1024);
    if (err == -1) {
        err_sys("listen error");
    }

    return listenfd;
}