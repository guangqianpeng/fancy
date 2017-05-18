//
// Created by frank on 17-2-12.
//

#include "connection.h"

static connection   *conns;
static connection   *peers;
static list         conn_list;

static void conn_set_upstream_addr(int size);
static void conn_init(connection *conn);
static void event_set_field(event *ev);

int conn_pool_init(mem_pool *p, int size)
{
    list_init(&conn_list);

    conns = palloc(p, size * sizeof(connection));
    ABORT_ON(conns, NULL);

    peers = palloc(p, size * sizeof(connection));
    ABORT_ON(peers, NULL);

    for (int i = size - 1; i >= 0; --i) {
        conns[i].read.conn = &conns[i];
        conns[i].write.conn = &conns[i];
        conns[i].peer = &peers[i];
        list_insert_head(&conn_list, &conns[i].node);

        peers[i].read.conn = &peers[i];
        peers[i].write.conn = &peers[i];
        peers[i].peer = &conns[i];
    }

    if (upstream_ip != NULL) {
        conn_set_upstream_addr(size);
    }

    return FCY_OK;
}

/* 从空闲链表中取出一个连接 */
connection *conn_get()
{
    list_node   *head;
    connection  *conn;

    if (list_empty(&conn_list)) {
        return NULL;
    }

    head = list_head(&conn_list);
    list_remove(head);

    conn = link_data(head, connection, node);
    conn_init(conn);
    conn_init(conn->peer);

    return conn;
}

void conn_free(connection *conn)
{
    conn->sockfd = -1;
    conn->peer->sockfd = -1;

    list_insert_head(&conn_list, &conn->node);
}

static void conn_set_upstream_addr(int size)
{
    assert(upstream_ip != NULL);

    struct sockaddr_in upstream_addr;
    int err;

    err = inet_pton(AF_INET, upstream_ip, &upstream_addr.sin_addr);
    ABORT_ON(err, 0);
    ABORT_ON(err, -1);

    upstream_addr.sin_family = AF_INET;
    upstream_addr.sin_port = htons(upstream_port);

    for (int i = 0; i < size; ++i) {
        peers[i].addr = upstream_addr;
    }
}

static void conn_init(connection *conn)
{
    conn->sockfd = -1;
    conn->app = NULL;
    conn->app_count = 0;

    event_set_field(&conn->read);
    event_set_field(&conn->write);
}

static void event_set_field(event *ev)
{
    connection *conn = ev->conn;

    bzero(ev, sizeof(event));
    ev->conn = conn;
}

int conn_enable_read(connection *conn, event_handler handler, uint32_t epoll_flag)
{
    assert(!conn->read.active);

    struct epoll_event e_event = {
            .data.ptr = conn,
            .events = epoll_flag | EPOLLIN | EPOLLRDHUP | EPOLLPRI
    };

    // conn->fd has already registered
    if (conn->write.active) {
        e_event.events |=  EPOLLOUT;
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->sockfd, &e_event), -1);
    }
    conn->read.active = 1;
    conn->read.handler = handler;
    return FCY_OK;
}

int conn_disable_read(connection *conn)
{
    assert(conn->read.active);

    if (conn->write.active) {

        struct epoll_event e_event = {
                .data.ptr = conn,
                .events = EPOLLOUT | EPOLLRDHUP | EPOLLPRI
        };
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->sockfd, NULL), -1);
    }

    conn->read.active = 0;
    return FCY_OK;
}

int conn_enable_write(connection *conn, event_handler handler, uint32_t epoll_flag)
{
    assert(!conn->write.active);

    struct epoll_event e_event = {
            .data.ptr = conn,
            .events = epoll_flag | EPOLLOUT | EPOLLRDHUP | EPOLLPRI
    };

    // conn->fd has already registered
    if (conn->read.active) {
        e_event.events |= EPOLLIN;
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->sockfd, &e_event), -1);
    }

    conn->write.active = 1;
    conn->write.handler = handler;
    return FCY_OK;
}

int conn_disable_write(connection *conn)
{
    assert(conn->write.active);

    if (conn->read.active) {
        struct epoll_event e_event = {
                .data.ptr = conn,
                .events =  EPOLLIN | EPOLLRDHUP | EPOLLPRI
        };
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->sockfd, NULL), -1);
    }

    conn->write.active = 0;
    return FCY_OK;
}