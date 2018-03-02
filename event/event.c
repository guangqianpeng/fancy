//
// Created by frank on 17-2-12.
//

#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>

#include "log.h"
#include "event.h"
#include "connection.h"

int epollfd = -1;

static struct epoll_event *event_list;

int event_init(mem_pool *p, int n_ev)
{
    assert(epollfd == -1);

    event_list = palloc(p, n_ev * sizeof(struct epoll_event));
    if (event_list == NULL) {
        return FCY_ERROR;
    }

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        LOG_SYSERR("epoll_create1 error");
        return FCY_ERROR;
    }

    epoll_events = n_ev;

    return FCY_OK;
}

int event_process(timer_msec timeout)
{
    int         n_ev, events;
    event       *revent, *wevent;
    connection  *conn;
    struct epoll_event *e_event;

    n_ev = epoll_wait(epollfd, event_list, epoll_events, (int)timeout);

    if (n_ev == -1) {
        if (errno == EINTR) {
            return 0;
        }
        LOG_SYSERR("epoll wait error");
        return FCY_ERROR;
    }
    else if (n_ev == 0) {
        /* timeout */
        return 0;
    }

    for (int i = 0; i < n_ev; ++i) {
        e_event = &event_list[i];
        events = e_event->events;
        conn = e_event->data.ptr;

        /* error detected, throw it to read_handler or write_handler */
        if (events & (EPOLLERR | EPOLLRDHUP)) {
            events |= EPOLLIN | EPOLLOUT ;
        }

        revent = &conn->read;
        if (revent->active && (events & EPOLLIN)) {
            revent->handler(revent);
        }

        wevent = &conn->write;
        if (wevent->active && (events & EPOLLOUT)) {
            // ignore timeout event
            if (conn->sockfd == -1) {
                continue;
            }
            wevent->handler(wevent);
        }
    }
    return n_ev;
}