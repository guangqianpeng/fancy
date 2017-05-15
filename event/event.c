//
// Created by frank on 17-2-12.
//

#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>

#include "event.h"



list        event_accept_post;
list        event_other_post;

static int n_events;
static int epollfd = -1;
static struct epoll_event *event_list;

int event_init(mem_pool *p, int n_ev)
{
    assert(epollfd == -1);

    event_list = palloc(p, n_ev * sizeof(struct epoll_event));
    if (event_list == NULL) {
        error_log("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        error_log("%s error at line %d\n", __FUNCTION__, __LINE__);
        exit(1);
    }

    list_init(&event_accept_post);
    list_init(&event_other_post);

    n_events = n_ev;

    return FCY_OK;
}

int event_process(timer_msec timeout)
{
    int         n_ev, events;
    event       *revent, *wevent;
    connection  *conn;
    struct epoll_event *e_event;

    n_ev = epoll_wait(epollfd, event_list, n_events, (int)timeout);

    if (n_ev == -1) {
        if (errno == EINTR) {
            error_log("epoll_wait EINTR");
            return 0;
        }
        error_log("epoll_wait error: %s", strerror(errno));
        return FCY_ERROR;
    }
    else if (n_ev == 0) {
        /* 超时 */
        return 0;
    }

    for (int i = 0; i < n_ev; ++i) {
        e_event = &event_list[i];
        events = e_event->events;
        conn = e_event->data.ptr;

        /* 检测到错误或者对端关闭连接，交给read_handler或者write_handler解决 */
        if (events & (EPOLLERR | EPOLLRDHUP)) {
            events |= EPOLLIN | EPOLLOUT ;
        }

        revent = &conn->read;
        if (revent->active && (events & EPOLLIN)) {
            revent->handler(revent);
        }

        wevent = &conn->write;
        if (wevent->active && (events & EPOLLOUT)) {
            // 忽略过期事件
            if (conn->fd == -1) {
                continue;
            }
            wevent->handler(revent);
        }
    }

    return n_ev;
}

int conn_enable_read(connection *conn, event_handler handler, uint32_t epoll_flag)
{
    assert(!conn->read->active);

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
    assert(conn->read->active);

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
    assert(!conn->write->active);

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