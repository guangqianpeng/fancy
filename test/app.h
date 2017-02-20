//
// Created by frank on 17-2-19.
//

#ifndef FANCY_APP_H
#define FANCY_APP_H

#include "event.h"

int init_server(int n_conn, int n_event);
int add_accept_event(uint16_t serv_port, event_handler accept_handler);

#endif //FANCY_APP_H
