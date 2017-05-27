//
// Created by frank on 17-5-27.
//

#include "base.h"

/* TODO: 可配置参数 */
int n_connections       = 10240;
int n_events            = 1024;
int request_per_conn    = 1000;
int request_timeout     = 5000;
int upstream_timeout    = 30000;
int serv_port           = 9877;
int single_process      = 0;
int n_workers           = 3;
int accept_defer        = 3;

/* upstream */
int         use_upstream = 1;
const char  *upstream_ip = "127.0.0.1";
uint16_t    upstream_port = 9878;

/* 静态文件匹配地址 */
const char *locations[] = {
        "/static/",
        "/fuck/",
        NULL
};
const char  *index_name = "index.html";
const char  *root = "/home/frank/ClionProjects/fancy/html";

Message     msg;

int         log_on = 1;
int         log_level = LOG_LEVEL_DEBUG;
int         log_fd = STDOUT_FILENO;