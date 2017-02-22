//
// Created by frank on 17-2-16.
//

#include "base.h"
#include "timer.h"
#include "conn_pool.h"
#include "request.h"
#include "app.h"

#define CONN_MAX            128
#define EVENT_MAX           128
#define REQUEST_TIMEOUT     30000
#define SERV_PORT           9877

int total_request = 0;

static void sig_handler(int signo);
static void accept_handler(event *ev);
static void read_handler(event *ev);
static void process_request_handler(event *ev);
static void write_headers_handler(event *ev);
static void write_body_handler(event *ev);
static void finalize_request_handler(event *ev);
static void empty_handler(event *ev);

static void close_connection(connection *conn);

int main()
{
    timer_msec  timeout;
    int         n_ev;

    if (init_server(CONN_MAX, EVENT_MAX) == FCY_ERROR) {
        err_quit("init_server error");
    }

    if (add_accept_event(SERV_PORT, accept_handler) == FCY_ERROR) {
        err_quit("add_accept_event error");
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sig_handler);

    printf("port %d\n", SERV_PORT);

    /* 事件循环 */
    while (1) {

        timeout = timer_recent();

        n_ev = event_process(timeout);

        if (n_ev == FCY_ERROR) {
            break;
        }

        timer_process();
    }

    err_msg("\nserver quit normally %d request", total_request);
}

static void sig_handler(int signo)
{
}

static void accept_handler(event *ev)
{
    int             connfd;
    struct sockaddr addr;
    socklen_t       len;
    connection      *conn;
    int             err;

    conn = conn_pool_get();
    if (conn == NULL) {
        err_msg("%s error at line %d: not enough free connections", __FUNCTION__, __LINE__);
        return;
    }

    inter:
    len = sizeof(addr);
    /* SOCK_NONBLOCK将connfd设为非阻塞 */
    connfd = accept4(ev->conn->fd, &addr, &len, SOCK_NONBLOCK);
    if (connfd == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                conn_pool_free(conn);
                return;
            default:
                err_sys("accept4 error");
        }
    }

    err_msg("new connection(%p) %d",conn, connfd);

    conn->fd = connfd;
    conn->read->handler = read_handler;
    conn->write->handler = empty_handler;

    err = event_conn_add(conn);
    if (err == -1) {
        err_quit("event_conn_add error");
    }

    timer_add(conn->read, REQUEST_TIMEOUT);

    /* 边沿触发必须读到EAGAIN为止 */
    accept_handler(ev);
}

static void read_handler(event *ev)
{
    ssize_t     n;
    connection  *conn;
    request     *rqst;
    buffer      *header_in;
    int         fd;
    int         err;

    conn = ev->conn;
    rqst = conn->app;
    fd = conn->fd;

    /* 处理超时 */
    if (ev->timeout) {
        if (rqst) {
            request_destroy(rqst);
        }

        err_msg("timeout");
        close_connection(conn);
        return;
    }

    /* 若是第一次调用则需要创建request */
    if (rqst == NULL) {
        rqst = conn->app = request_create(conn);
        if (rqst == NULL) {
            err_quit("request_create error");
        }
    }

    header_in = rqst->header_in;

    /* read buffer满 */
    if (buffer_full(header_in)) {
        rqst->status_code = HTTP_R_URI_TOO_LONG;
        goto error;
    }

    assert(buffer_empty(rqst->header_out));

    /* 读http请求 */
    eintr:
    n = read(fd, header_in->data_end, header_in->end - header_in->data_end);

    switch (n) {
        case -1:
            if (errno == EINTR) {
                goto eintr;
            }
            if (errno == EAGAIN) {
                if (!ev->timer_set) {
                    timer_add(ev, REQUEST_TIMEOUT);
                }
                return;
            }
            if (errno != ECONNRESET) {
                err_sys("read error");
            }

            /* fall through */
        case 0:
            /* 对端在没有发送完整请求的情况下关闭连接 */
            err_msg("read %s", n == 0 ? "FIN" : "RESET");
            request_destroy(rqst);
            close_connection(conn);
            return;

        default:
            break;
    }

    buffer_seek_end(header_in, (int)n);

    /* 解析请求 */
    err = parse_request(rqst);
    switch (err) {
        case FCY_ERROR:
            err_msg("parse_request error");
            goto error;

        case FCY_AGAIN:
            read_handler(ev);
            return;

        case FCY_OK:
            break;

        default:
            assert(0);
    }

    /* 解析完毕 */
    if (ev->timer_set) {
        timer_del(ev);
    }

    ev->handler = process_request_handler;
    process_request_handler(ev);
    return;

    error:
    rqst->keep_alive = 0;
    ev->handler = empty_handler;
    conn->write->handler = write_headers_handler;
    write_headers_handler(conn->write);
    return;
}

static void process_request_handler(event *ev)
{
    connection  *conn;
    request     *rqst;
    int         err;

    conn = ev->conn;
    rqst = conn->app;

    err = check_request_header_filed(rqst);

    if (err == FCY_OK && rqst->is_static) {
        err = process_request_static(rqst);
    }

    if (rqst->status_code == HTTP_R_OK && !rqst->is_static) {
        rqst->status_code = HTTP_R_NOT_FOUND;
    }

    if (err == FCY_ERROR) {
        rqst->keep_alive = 0;
    }

    ev->handler = empty_handler;
    conn->write->handler = write_headers_handler;
    write_headers_handler(conn->write);
    return;
}

static void write_headers_handler(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_out;
    const char  *status_str;
    ssize_t     n;

    conn = ev->conn;
    rqst = conn->app;
    header_out = rqst->header_out;
    status_str = status_code_out_str[rqst->status_code];

    /* 写header_out */
    if (buffer_empty(header_out)) {
        /* response line */
        buffer_write(header_out, "HTTP/1.1 ", 9);
        buffer_write(header_out, status_str, strlen(status_str));
        buffer_write(header_out, "\r\nServer: Fancy", 15);
        buffer_write(header_out, "\r\nContent-Type: ", 16);

        if (rqst->status_code == HTTP_R_OK) {
            buffer_write(header_out, rqst->content_type,
                         strlen(rqst->content_type));
            buffer_write(header_out, "\r\nContent-Length: ", 18);
            n = sprintf((char *) header_out->data_end, "%ld", rqst->sbuf.st_size);
            buffer_seek_end(header_out, (int) n);
        }
        else {
            buffer_write(header_out, "text/html; charset=utf-8", 24);
            buffer_write(header_out, "\r\nContent-Length: ", 18);
            n = sprintf((char *) header_out->data_end, "%ld", strlen(status_str));
            buffer_seek_end(header_out, (int) n);
        }

        if (rqst->keep_alive) {
            buffer_write(header_out, "\r\nConnection: keep-alive\r\n\r\n", 28);
        }
        else {
            buffer_write(header_out, "\r\nConnection: close\r\n\r\n", 23);
        }

        if (rqst->status_code != HTTP_R_OK) {
            buffer_write(header_out, status_str, strlen(status_str));
        }
    }

    while (!buffer_empty(header_out)) {
        /* TODO: 按照muduo库作者陈硕的说法:
         * 第二次write调用几乎肯定会返回EAGAIN, 因此不必写成循环
         * 此处留作以后优化
         * */
        inter:
        n = write(conn->fd, header_out->data_start, buffer_size(header_out));
        if (n == -1) {
            switch (errno) {
                case EINTR:
                    goto inter;
                case EAGAIN:
                    return;
                case EPIPE:
                case ECONNRESET:
                    request_destroy(rqst);
                    err_msg("send response head error %s", errno == EPIPE ? "EPIPE" : "ERESET");
                    close_connection(conn);
                    return;
                default:
                    err_sys("write error");
            }
        }
        buffer_seek_start(header_out, (int) n);
    }

    if (rqst->send_fd > 0) {
        ev->handler = write_body_handler;
    }
    else {
        ev->handler = finalize_request_handler;
    }

    ev->handler(ev);
}

static void write_body_handler(event *ev)
{
    connection  *conn;
    request     *rqst;
    struct stat *sbuf;
    ssize_t     n;

    conn = ev->conn;
    rqst = conn->app;
    sbuf = &rqst->sbuf;

    while(sbuf->st_size > 0) {
        inter:
        n = sendfile(conn->fd, rqst->send_fd, NULL, (size_t) sbuf->st_size);
        if (n == -1) {
            switch (errno) {
                case EINTR:
                    goto inter;
                case EAGAIN:
                    return;
                case EPIPE:
                case ECONNRESET:
                    request_destroy(rqst);

                    err_msg("sendfile error %s", errno == EPIPE ? "EPIPE" : "ERESET");
                    close_connection(conn);
                    return;
                default:
                    err_sys("sendfile error");
            }
        }
        sbuf->st_size -= n;
    }

    ev->handler = finalize_request_handler;
    finalize_request_handler(ev);
}

static void finalize_request_handler(event *ev)
{
    connection  *conn;
    request     *rqst;

    conn = ev->conn;
    rqst = conn->app;

    /*
    if (rqst->parse_state != error_) {
        request_print(rqst);
    }
   */

    request_destroy(rqst);

    ++conn->app_count;

    if (!rqst->keep_alive) {
        err_msg(status_code_out_str[rqst->status_code]);
        close_connection(conn);
        return;
    }

    ev->handler = empty_handler;
    conn->read->handler = read_handler;
    read_handler(conn->read);
    return;
}

static void empty_handler(event *ev)
{
}

static void close_connection(connection *conn)
{
    int fd = conn->fd;

    total_request += conn->app_count;

    if (event_conn_del(conn) == -1) {
        err_quit("event_conn_del error");
    }

    if (conn->read->timer_set) {
        timer_del(conn->read);
    }

    if (close(fd) == -1) {
        err_sys("close error");
    }

    conn_pool_free(conn);

    err_msg("close connection(%d) %d", conn->app_count, fd);
}