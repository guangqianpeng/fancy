//
// Created by frank on 17-2-19.
//

#include <sys/epoll.h>

#include "app.h"
#include "Signal.h"
#include "connection.h"
#include "timer.h"
#include "../http/request.h"

/* TODO: 可配置参数 */
int n_connections       = 10240;
int n_events            = 1024;
int request_per_conn    = 1000;
int request_timeout     = 5000;
int upstream_timeout    = 3000;
int serv_port           = 9877;
int single_process      = 1;
int n_workers           = 3;
int accept_defer        = 10;

/* upstream 地址 */
int         use_upstream = 1;
const char  *upstream_ip = "127.0.0.1";
uint16_t    upstream_port = 9877;

/* 静态文件匹配地址 */
const char *locations[] = {
        //"/static/",
        "/fuck/",
        NULL
};
const char *index_name = "index.html";
const char *root = ".";

static int tcp_listen();
static int init_and_add_accept_event(event_handler accept_handler_);

int init_worker(event_handler accept_handler)
{
    mem_pool    *pool;
    size_t      size;

    size = n_connections * sizeof (connection) + sizeof(mem_pool);
    pool = mem_pool_create(size);

    if (pool == NULL){
        return FCY_ERROR;
    }

    if (conn_pool_init(pool, n_connections) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (event_init(pool, n_events) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    timer_init();

    if (request_init(pool) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (init_and_add_accept_event(accept_handler) == FCY_ERROR) {
        error_log("add_aceept_event error");
        return FCY_ERROR;
    }

    Signal(SIGPIPE, SIG_IGN);

    return FCY_OK;
}


void event_and_timer_process()
{
    timer_msec  timeout = (timer_msec)-1;
    int         n_ev;

    while (1) {

        n_ev = event_process(timeout);
        if (n_ev == FCY_ERROR) {
            error_log("event_process error");
            return;
        }

        timer_expired_process();
        timeout = timer_recent();
    }
}

static int tcp_listen()
{
    int                 listenfd;
    struct sockaddr_in  servaddr;
    socklen_t           addrlen;
    const int           on = 1;
    int                 err;

    listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    ABORT_ON(listenfd, -1);

    err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    ABORT_ON(err, -1);

    err = setsockopt(listenfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &accept_defer, sizeof(accept_defer));
    ABORT_ON(err, -1);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    servaddr.sin_port           = htons((uint16_t)serv_port);

    addrlen = sizeof(servaddr);
    err = bind(listenfd, (struct sockaddr*)&servaddr, addrlen);
    ABORT_ON(err, -1);

    err = listen(listenfd, 1024);
    ABORT_ON(err, -1);

    return listenfd;
}

int init_and_add_accept_event(event_handler accept_handler)
{
    connection  *conn;

    conn = conn_get();
    ABORT_ON(conn, NULL);

    conn->sockfd = tcp_listen();
    conn->read.handler = accept_handler;

    /* 水平触发 */
    ABORT_ON(conn_enable_read(conn, accept_handler, 0), FCY_ERROR);

    return FCY_OK;
}