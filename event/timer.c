//
// Created by frank on 17-2-12.
//

#include <assert.h>
#include "timer.h"

#define TIMER_INFINITE (timer_msec)-1;

static rbtree      timer;
static rbtree_node sentinel;

void timer_init()
{
    rbtree_init(&timer, &sentinel);
}

void timer_add(event *ev, timer_msec timeout)
{
    assert(!ev->timer_set);

    timer_msec      key;

    key = timeout + current_msec();

    ev->rb_node.key = key;
    rbtree_insert(&timer, &ev->rb_node);

    assert(!ev->timeout);
    ev->timer_set = 1;
}

void timer_del(event *ev)
{
    assert(ev->timer_set);

    rbtree_delete(&timer, &ev->rb_node);

    ev->timer_set = 0;
    ev->timeout = 0;
}

timer_msec timer_recent()
{
    rbtree_node *recent;
    timer_msec  current;

    /* 计时器为空, 表示不会有任何事件发生 */
    if (rbtree_empty(&timer)) {
        return TIMER_INFINITE;
    }

    recent = rbtree_min(&timer);
    current = current_msec();
    if (current < recent->key) {    /* 没有事件发生，返回最近事件的时间差值 */
        return recent->key - current;
    }

    /* 有事件发生 */
    return 0;
}

void timer_expired_process()
{
    timer_msec      current;
    event           *ev;
    rbtree_node     *node;

    current = current_msec();

    while (!rbtree_empty(&timer)) {
        node = rbtree_min(&timer);
        if (current < node->key) {
            break;
        }

        rbtree_delete(&timer, node);

        ev = link_data(node, event, rb_node);
        ev->timer_set = 0;
        ev->timeout = 1;

        ev->handler(ev);
    }
}

timer_msec current_msec()
{
    struct timeval now;

    if (gettimeofday(&now, NULL) == -1) {
        err_sys("gettimeofday error");
    }

    return (timer_msec)now.tv_sec * 1000 + now.tv_usec / 1000;
}