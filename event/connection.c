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
    if (conns == NULL) {
        return FCY_ERROR;
    }

    peers = palloc(p, size * sizeof(connection));
    if (peers == NULL) {
        return FCY_ERROR;
    }

    for (int i = size - 1; i >= 0; --i) {
        conns[i].read.conn = &conns[i];
        conns[i].write.conn = &conns[i];
        conns[i].peer = &peers[i];
        list_insert_head(&conn_list, &conns[i].node);

        peers[i].read.conn = &peers[i];
        peers[i].write.conn = &peers[i];
        peers[i].peer = &conns[i];
    }

    if (use_upstream) {
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

char *conn_str(connection *conn)
{
    static char buf[32];
    snprintf(buf, 32,  "[%s:%hu]",
             inet_ntoa(conn->addr.sin_addr),
             ntohs(conn->addr.sin_port));
    return buf;
}

void conn_enable_accept(connection *conn, event_handler handler)
{
    assert(!conn->read.active);

    /* 水平触发 */
    struct epoll_event e_event = {
            .data.ptr = conn,
            .events = EPOLLIN | EPOLLRDHUP | EPOLLPRI
    };

    CHECK(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->sockfd, &e_event));

    conn->read.active = 1;
    conn->read.handler = handler;
}

void conn_enable_read(connection *conn, event_handler handler)
{
    assert(!conn->read.active);

    struct epoll_event e_event = {
            .data.ptr = conn,
            .events = EPOLLET | EPOLLIN | EPOLLRDHUP | EPOLLPRI
    };

    // conn->fd has already registered
    if (conn->write.active) {
        e_event.events |=  EPOLLOUT;
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event));
    }
    else {
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->sockfd, &e_event));
    }
    conn->read.active = 1;
    conn->read.handler = handler;
}

void conn_disable_read(connection *conn)
{
    assert(conn->read.active);

    if (conn->write.active) {

        struct epoll_event e_event = {
                .data.ptr = conn,
                .events = EPOLLOUT | EPOLLRDHUP | EPOLLPRI
        };
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event));
    }
    else {
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->sockfd, NULL));
    }

    conn->read.active = 0;
}

void conn_enable_write(connection *conn, event_handler handler)
{
    assert(!conn->write.active);

    struct epoll_event e_event = {
            .data.ptr = conn,
            .events = EPOLLET | EPOLLOUT | EPOLLRDHUP | EPOLLPRI
    };

    // conn->fd has already registered
    if (conn->read.active) {
        e_event.events |= EPOLLIN;
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event));
    }
    else {
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->sockfd, &e_event));
    }

    conn->write.active = 1;
    conn->write.handler = handler;
}

void conn_disable_write(connection *conn)
{
    assert(conn->write.active);

    if (conn->read.active) {
        struct epoll_event e_event = {
                .data.ptr = conn,
                .events =  EPOLLIN | EPOLLRDHUP | EPOLLPRI
        };
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_MOD, conn->sockfd, &e_event));
    }
    else {
        CHECK(epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->sockfd, NULL));
    }

    conn->write.active = 0;
}

int conn_read(connection *conn, buffer *in)
{
    int n;

    inter:
    n = (int)read(conn->sockfd, in->data_end, buffer_space(in));
    switch(n) {
        case -1:
        {
            switch (errno) {
                case EINTR:
                    goto inter;
                case EAGAIN:
                    return FCY_AGAIN;
                default:
                    LOG_SYSERR("%s read error", conn_str(conn));
                    return FCY_ERROR;
            }
        }
        case 0:
            LOG_DEBUG("%s get fin", conn_str(conn));
            return FCY_ERROR;

        default:
            buffer_seek_end(in, n);
            return FCY_OK;
    }
}

int conn_write(connection *conn, buffer *out)
{
    int n;

    inter:
    n = (int)write(conn->sockfd, out->data_start, buffer_size(out));
    if (n == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                return FCY_AGAIN;
            default:
                LOG_SYSERR("%s write error", conn_str(conn));
                return FCY_ERROR;
        }
    }
    buffer_seek_start(out, n);
    return FCY_OK;
}

int conn_send_file(connection *conn, int fd, struct stat *st)
{
    ssize_t n;

    inter:
    n = sendfile(conn->sockfd, fd, NULL, INT_MAX);
    if (n == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                return FCY_AGAIN;
            default:
                LOG_SYSERR("%s sendfile error", conn_str(conn));
                return FCY_ERROR;
        }
    }
    st->st_size -= n;
    return FCY_OK;
}


static void conn_set_upstream_addr(int size)
{
    assert(upstream_ip != NULL);

    struct sockaddr_in upstream_addr;
    int err;

    err = inet_pton(AF_INET, upstream_ip, &upstream_addr.sin_addr);
    if (err == 0) {
        LOG_FATAL("invalid network address %s", upstream_ip);
    }
    assert(err == 1);

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