//
// Created by frank on 17-2-12.
//

#ifndef FANCY_EVENT_H
#define FANCY_EVENT_H

#include "base.h"
#include "buffer.h"
#include "list.h"
#include "rbtree.h"
#include "palloc.h"

extern  list event_accept_post;
extern  list event_other_post;

typedef rbtree_key          timer_msec;
typedef struct event        event;
typedef struct connection   connection;
typedef void (*event_handler)(event *);

timer_msec current_msec();

struct event {
    unsigned        read;       // 应用层(http)可读, 例如一个完整的请求抵达
    unsigned        write;      // 应用层可写
    unsigned        accept;
    unsigned        active;     // 是否在epoll_wait中
    unsigned        timer_set;  // 是否在定时器中
    unsigned        timeout;    // 是否为超时事件

    rbtree_node     rb_node;
    list_node       l_node;

    event_handler   handler;

    connection      *conn;
};

struct connection {
    int                 fd;
    event               *read;
    event               *write;

    void                *app;   // http, echo

    struct sockaddr_in  addr;

    mem_pool            *pool;
    buffer              *buf;

    list_node           node;
};

int event_init(mem_pool *p, int n_ev);  // n_events是epoll返回的最大事件数目
int event_add(event *ev, int flag);
int event_del(event *ev, int flag);
int event_mod(event *ev, int flag);
int event_conn_add(connection *conn);
int event_conn_del(connection *conn);

/* -1   被信号中断
 * 0    超时(没有处理任何事件)
 * >0   处理掉事件数 */
int event_process(timer_msec timeout, int post_events);

void event_process_posted(list *events);
#endif //FANCY_EVENT_H