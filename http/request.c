//
// Created by frank on 17-2-16.
//

#include <assert.h>
#include "request.h"

const char *status_code_out_str[] = {
        "200 OK",
        "400 Bad Request",
        "403 Forbidden",
        "404 Not Found",
        "408 Request Time-out",
        "411 Length Required",
        "413 Payload Too Large",
        "414 URI Too Long",
        "431 Request Header Fields Too Large",
        "500 Internal Server Error",
        "501 Not Implemented",
};

static void set_cork(connection *conn, int open);

request *request_create(connection *c)
{
    request     *r;
    mem_pool    *p;

    p = mem_pool_create(HTTP_POOL_SIZE);
    if (p == NULL) {
        return NULL;
    }

    r = pcalloc(p, sizeof(*r));
    if (r == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->header_in = buffer_create(p, HTTP_HEADER_SIZE);
    r->header_out = buffer_create(p, HTTP_HEADER_SIZE);
    if (r->header_in == NULL ||
        r->header_out == NULL) {
        mem_pool_destroy(p);
        return NULL;
    }

    r->conn = c;
    r->pool = p;

    assert(c->app == NULL);  /* 同一时刻只允许一个app占用connection */
    c->app = r;

    /* 打开TCP_CORK选项 */
    set_cork(c, 1);

    return r;
}

void request_destroy(request *r)
{
    /* 关闭TCP_CORK选项 */
    set_cork(r->conn, 0);

    r->conn->app = NULL;
    if (r->send_fd > 0) {
        close(r->send_fd);
    }

    mem_pool_destroy(r->pool);
}

void request_print(request *r)
{
    printf("method: %d\n", r->method);
    printf("version: %d\n", r->version);
    printf("keep-alive: %d\n", r->keep_alive);
    printf("content-length: %ld\n", r->cnt_len);

    printf("has_args: %d\n", r->has_args);
    printf("has_host_header: %d\n", r->has_host_header);
    printf("has_content_length_header: %d\n", r->has_content_length_header);

    printf("suffix: %s\n", r->suffix);
    printf("is_static: %d\n", r->is_static);
    printf("content_type: %s\n", r->content_type);

    printf("%s\n\n", status_code_out_str[r->status_code]);
}

static void set_cork(connection *conn, int open)
{
    int err;

    err = setsockopt(conn->fd, IPPROTO_TCP, TCP_CORK, &open, sizeof(open));
    if (err == -1) {
        logger_client(&conn->addr, "setsockopt error %s", strerror(errno));
    }
}
