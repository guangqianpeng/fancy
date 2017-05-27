//
// Created by frank on 17-5-16.
//

#ifndef FANCY_HTTP_H
#define FANCY_HTTP_H

#include "event.h"

int init_and_add_accept_event(event_handler accept_handler_);
void accept_h(event *);

#endif //FANCY_HTTP_H
