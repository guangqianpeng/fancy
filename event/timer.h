//
// Created by frank on 17-2-12.
//

#ifndef FANCY_TIMER_H
#define FANCY_TIMER_H

#include "event.h"

void timer_init();
void timer_add(event *ev, timer_msec timeout);
void timer_del(event *ev);
void timer_expired_process();
void event_and_timer_process();

#endif //FANCY_TIMER_H
