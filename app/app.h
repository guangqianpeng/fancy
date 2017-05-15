//
// Created by frank on 17-2-19.
//

#ifndef FANCY_APP_H
#define FANCY_APP_H

#include "event.h"

/* TODO: 参数应该是可配置的 */

extern int n_connections;       // 并发连接数
extern int n_events;            // 一次循环处理事件数
extern int request_per_conn;    // 每个连接最多处理多少个请求
extern int request_timeout;     // 请求超时的上限

extern int serv_port;           // 端口号

extern int single_process;      // 是否单进程
extern int n_workers;           // 多进程下workers数目

int init_worker(event_handler accept_handler);

void event_and_timer_process();
#endif //FANCY_APP_H
