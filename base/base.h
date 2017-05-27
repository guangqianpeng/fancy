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

#include "log.h"

#define FCY_OK      0
#define FCY_ERROR   -1
#define FCY_AGAIN   EAGAIN

#define link_data(node, type, member) \
    (type*)((u_char*)node - offsetof(type, member))

#ifdef NDEBUG
__BEGIN_DECLS
extern void __assert_perror_fail (int errnum,
                                  const char *file,
                                  unsigned int line,
                                  const char *function)
__THROW __attribute__ ((__noreturn__));
__END_DECLS
#endif

#define CHECK(ret) ({ __typeof__ (ret) errnum = (ret);         \
                        if (__builtin_expect(errnum != 0, 0))    \
                            __assert_perror_fail (errnum, __FILE__, __LINE__, __func__);})

/* TODO: 参数应该是可配置的 */
extern int n_connections;       // 并发连接数
extern int n_events;            // 一次循环处理事件数
extern int request_per_conn;    // 每个连接最多处理多少个请求
extern int request_timeout;     // 请求超时的上限
extern int upstream_timeout;    // 请求上游响应超时

extern int serv_port;           // 端口号

extern int single_process;      // 是否单进程
extern int n_workers;           // 多进程下workers数目

extern int accept_defer;

extern const char *locations[];        // 静态文件匹配的文件地址
extern const char *index_name;         // 索引文件名称
extern const char *root;               // 根目录

/* upstream 地址 */
extern int          use_upstream;
extern const char   *upstream_ip;
extern uint16_t     upstream_port;

extern int          log_on;
extern int          log_level;
extern int          log_fd;

/* 每个进程各一份 */
typedef struct Message Message;
struct Message {
    int worker_id;
    int total_connection;
    int total_request;
    int ok_request;
};
extern Message msg;

#endif //FANCY_BASE_H
