//
// Created by frank on 17-5-30.
//

#ifndef FANCY_CONFIG_H
#define FANCY_CONFIG_H

#include "palloc.h"

#define FANCY_PREFIX        "/home/frank/ClionProjects/fancy/html/"
#define FANCY_PID_FILE      FANCY_PREFIX"/fancy.pid"
#define FANCY_CONFIG_FILE   FANCY_PREFIX"fancy.conf"

void config(const char *path);

#endif //FANCY_CONFIG_H
