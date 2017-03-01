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
        err_msg("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        err_sys("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    list_init(&event_accept_post);
    list_init(&event_other_post);

    n_events = n_ev;

    return FCY_OK;
}

int event_add(event *ev, int flag)
{
    int                 op;
    connection          *conn;
    struct epoll_event  e_event;

    assert(!ev->active);

    conn = ev->conn;
    e_event.data.ptr = conn;

    /* 加入的事件为读事件 */
    if (conn->read == ev) {
        e_event.events = EPOLLIN | flag;

        /* connection已经被监控 */
        if (conn->write->active) {
            e_event.events |= EPOLLOUT;
            op = EPOLL_CTL_MOD;
        }
        else {
            op = EPOLL_CTL_ADD;
        }
    }
        /* 加入的事件为写事件 */
    else if (conn->write == ev) {
        e_event.events = EPOLLOUT | flag;

        /* connection已经被监控 */
        if (conn->read->active) {
            e_event.events |= EPOLLIN;
            op = EPOLL_CTL_MOD;
        }
        else {
            op = EPOLL_CTL_ADD;
        }
    }
    else {
        err_quit("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    if (epoll_ctl(epollfd, op, conn->fd, &e_event) == -1) {
        err_msg("%d", conn->fd);
        err_sys("%s error at line %d", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    ev->active = 1;

    return FCY_OK;
}

int event_del(event *ev, int flag)
{
    int                 op;
    connection          *conn;
    struct epoll_event  e_event;

    assert(ev->active);

    conn = ev->conn;
    e_event.data.ptr = conn;

    if (conn->read == ev) {
        if (conn->write->active) {
            e_event.events = EPOLLOUT | flag;
            op = EPOLL_CTL_MOD;
        }
        else {
            e_event.events = 0;
            op = EPOLL_CTL_DEL;
        }
    }
    else if (conn->write == ev) {
        if (conn->read->active) {
            e_event.events = EPOLLIN | flag;
            op = EPOLL_CTL_MOD;
        }
        else {
            e_event.events = 0;
            op = EPOLL_CTL_DEL;
        }
    }
    else {
        err_quit("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    if (epoll_ctl(epollfd, op, conn->fd, &e_event) == -1) {
        err_sys("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    if (op == EPOLL_CTL_DEL) {
        conn->fd = -1;
    }
    ev->active = 0;

    return FCY_OK;
}

int event_process(timer_msec timeout, int post_events)
{
    int         n_ev, events;
    event       *revent, *wevent;
    connection  *conn;
    struct epoll_event *e_event;

    n_ev = epoll_wait(epollfd, event_list, n_events, timeout);

    if (n_ev == -1) {
        if (errno == EINTR) {
            logger("epoll_wait EINTR");
            return 0;
        }
        logger("epoll_wait error: %s", strerror(errno));
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

        revent = conn->read;
        if (revent->active && (events & EPOLLIN)) {
            if (post_events) {
                if (revent->accept) {
                    list_insert_head(&event_accept_post, &revent->l_node);
                }
                else {
                    list_insert_head(&event_other_post, &revent->l_node);
                }
            }
            else {
                revent->handler(revent);
            }
        }

        wevent = conn->write;
        if (wevent->active && (events & EPOLLOUT)) {
            // TODO: 忽略过期事件
            if (conn->fd == -1) {
                continue;
            }

            if (post_events) {
                list_insert_head(&event_other_post, &wevent->l_node);
            }
            else {
                wevent->handler(revent);
            }
        }
    }

    return n_ev;
}

void event_process_posted(list *events)
{
    list_node   *x;
    event       *ev;

    while (!list_empty(events)) {
        x = list_head(events);

        ev = link_data(x, event, l_node);
        ev->handler(ev);

        list_remove(x);
    }
}

int event_conn_add(connection *conn)
{
    assert(!conn->read->active && !conn->write->active);

    struct epoll_event e_event;

    e_event.data.ptr = conn;
    e_event.events = EPOLLPRI | EPOLLIN | EPOLLOUT | EPOLLET | EPOLLRDHUP;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn->fd, &e_event) == -1) {
        err_sys("%s error at line %d\n", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    conn->read->active = 1;
    conn->write->active = 1;

    return FCY_OK;
}

int event_conn_del(connection *conn)
{
    assert(conn->read->active || conn->write->active);

    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, conn->fd, NULL) == -1) {
        err_sys("%s error at line %d", __FUNCTION__, __LINE__);
        return FCY_ERROR;
    }

    conn->fd = -1; // 过期
    conn->read->active = 0;
    conn->write->active = 0;

    return FCY_OK;
}