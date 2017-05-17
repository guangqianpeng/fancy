//
// Created by frank on 17-2-12.
//

#include "connection.h"

static connection   *conns;
static connection   *peers;
static list         conn_list;

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
    conn->fd = -1;
    conn->peer->fd = -1;

    list_insert_head(&conn_list, &conn->node);
}

static void conn_init(connection *conn)
{
    conn->fd = -1;
    conn->app = NULL;
    conn->app_count = 0;
    bzero(&conn->addr, sizeof(conn->addr));

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
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->fd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->fd, &e_event), -1);
    }
    conn->read.active = 1;
    conn->read.handler = handler;
    return FCY_OK;
}

int conn_disable_read(connection *conn, uint32_t epoll_flag)
{
    assert(conn->read.active);

    if (conn->write.active) {

        struct epoll_event e_event = {
                .data.ptr = conn,
                .events = epoll_flag | EPOLLOUT | EPOLLRDHUP | EPOLLPRI
        };
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->fd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL), -1);
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
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->fd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->fd, &e_event), -1);
    }

    conn->write.active = 1;
    conn->write.handler = handler;
    return FCY_OK;
}

int conn_disable_write(connection *conn, uint32_t epoll_flag)
{
    assert(conn->write.active);

    if (conn->read.active) {
        struct epoll_event e_event = {
                .data.ptr = conn,
                .events = epoll_flag | EPOLLIN | EPOLLRDHUP | EPOLLPRI
        };
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->fd, &e_event), -1);
    }
    else {
        RETURN_ON(epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL), -1);
    }

    conn->write.active = 0;
    return FCY_OK;
}