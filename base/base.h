//
// Created by frank on 17-2-12.
//

#ifndef FANCY_BASE_H
#define FANCY_BASE_H

#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/epoll.h>

#include <signal.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "error.h"

#define FCY_OK      0
#define FCY_ERROR   -1
#define FCY_AGAIN   EAGAIN

#define link_data(node, type, member) \
    (type*)((u_char*)node - offsetof(type, member))

#define RETURN_ON(exp, err)   \
do {    \
    if (exp == err) {    \
        err_sys("%s error at line %d", __FUNCTION__, __LINE__); \
        return FCY_ERROR;   \
    }   \
} while(0)

#define ABORT_ON(exp, err)    \
do {    \
    if (exp == err) {    \
        err_sys("%s error at line %d", __FUNCTION__, __LINE__); \
        abort();    \
    }   \
} while(0)


/* 全局配置 */
extern int n_connections;       // 并发连接数
extern int n_events;            // 一次循环处理事件数
extern int request_per_conn;    // 每个连接最多处理多少个请求
extern int request_timeout;     // 请求超时的上限

extern int serv_port;           // 端口号

extern int single_process;      // 是否单进程
extern int n_workers;           // 多进程下workers数目

extern const char *locations[];     // 静态文件匹配的文件地址
extern const char *index_name;      // 索引文件名称
extern const char *root;            // 根目录


/* upstream 地址 */
extern int          use_upstream;
extern const char   *upstream_ip;
extern uint16_t     upstream_port;

#endif //FANCY_BASE_H
