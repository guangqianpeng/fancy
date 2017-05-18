//
// Created by frank on 17-5-16.
//

#ifndef FANCY_HTTP_H
#define FANCY_HTTP_H

#include "event.h"

void accept_h(event *);

void read_request_headers_h(event *);

void parse_request_h(event *);

void read_request_body(event *);

void process_request_h(event *);

/* 专门处理动态内容的 handler */
void peer_connect_h(event *);
void upstream_write_request_h(event *);
void upstream_read_response_h(event *);
void upstream_write_response_h(event *);

void write_response_headers_h(event *);

void send_file_h(event *);

/* 表示一个请求处理完成，可能关闭连接，也可能keep_alive */
void finalize_request_h(event *);

#endif //FANCY_HTTP_H
