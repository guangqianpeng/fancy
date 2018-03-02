//
// Created by frank on 17-2-12.
//

#ifndef FANCY_EVENT_H
#define FANCY_EVENT_H

#include "palloc.h"
#include "rbtree.h"

extern int epollfd;

typedef rbtree_key          timer_msec;
typedef struct event        event;
typedef struct connection   connection;
typedef void (*event_handler)(event *);

struct event {

    unsigned        active:1;     // is in epoll_wait ?
    unsigned        timer_set:1;  // is in timer ?
    unsigned        timeout:1;    // is timeout ?

    rbtree_node     rb_node;

    event_handler   handler;

    connection      *conn;
};

int event_init(mem_pool *p, int n_ev);  // n_events是epoll返回的最大事件数目

/* -1   被信号中断
 * 0    超时(没有处理任何事件)
 * >0   处理掉事件数 */
int event_process(timer_msec timeout);

#endif //FANCY_EVENT_H