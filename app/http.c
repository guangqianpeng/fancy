//
// Created by frank on 17-2-16.
//

#include "base.h"
#include "Signal.h"
#include "timer.h"
#include "conn_pool.h"
#include "request.h"
#include "app.h"

static int localfd[2];

static struct {
    int worker_id;
    int total_connection;
    int total_request;
    int ok_request;
} msg, total;

static void sig_empty_handler(int signo);
static void sig_quit_handler(int signo);

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
    int     err;

    err = init_server();
    if (err == FCY_ERROR) {
        error_log("init server error");
        exit(1);
    }

    /* 单进程模式 */
    if (single_process) {

        err = init_worker(accept_handler);
        if (err == FCY_ERROR) {
            error_log("init_worker error");
            exit(1);
        }

        error_log("single listening port %d", serv_port);

        Signal(SIGINT, sig_quit_handler);

        event_and_timer_process();

        error_log("single quit");
        exit(0);
    }

    /* 多进程模式 */
    err = socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, localfd);
    if (err == -1) {
        error_log("socketpair error", strerror(errno));
        exit(1);
    }

    for (int i = 1; i <= n_workers; ++i) {
        err = fork();
        switch (err) {
            case -1:
                error_log("fork error");
                exit(1);

            case 0:

                msg.worker_id = i;

                if (close(localfd[0]) == -1 ) {
                    error_log("worker %d close error %s", i, strerror(errno));
                    exit(1);
                }

                err = init_worker(accept_handler);
                if (err == FCY_ERROR) {
                    error_log("worker %d init worker error", i);
                    exit(1);
                }

                error_log("worker %d listening port %d", i, serv_port);

                Signal(SIGPIPE, SIG_IGN);
                Signal(SIGINT, sig_quit_handler);

                event_and_timer_process();

                /* never reach here */
                assert(1);
                exit(1);

            default:
                break;
        }
    }

    Signal(SIGINT, sig_empty_handler);

    if (close(localfd[1]) == -1) {
        error_log("master close error %s", strerror(errno));
        exit(1);
    }

    for (int i = 1; i <= n_workers; ++i) {
        inter_wait:
        switch (wait(NULL)) {
            case -1:
                if (errno == EINTR) {
                    goto inter_wait;
                }
                else {
                    error_log("master wait error %s", strerror(errno));
                    exit(1);
                }

            default:
            inter_read:
                if (read(localfd[0], &msg, sizeof(msg)) != sizeof(msg)) {
                    if (errno == EINTR) {
                        goto inter_read;
                    }
                    else if (err == EAGAIN) {
                        error_log("master got bad worker");
                    }
                    else {
                        error_log("master read error", strerror(errno));
                        exit(1);
                    }
                }

                total.total_connection += msg.total_connection;
                total.total_request += msg.total_request;
                total.ok_request += msg.ok_request;

                error_log("worker %d quit:"
                                  "\n\tconnection=%d\n\tok_request=%d\n\tother_request=%d",
                          msg.worker_id, msg.total_connection, msg.ok_request, msg.total_request - msg.ok_request);

                break;
        }
    }

    error_log("master quit normally:"
                      "\n\tconnection=%d\n\tok_request=%d\n\tother_request=%d",
              total.total_connection, total.ok_request, total.total_request - total.ok_request);
    exit(0);
}

static void sig_quit_handler(int signo) {
    ssize_t err;

    err = write(localfd[1], &msg, sizeof(msg));
    if (err != sizeof(msg)) {
        err = write(STDERR_FILENO, "worker write failed\n", 20);
        (void)err;
    }

    exit(0);
}

static void sig_empty_handler(int signo)
{
}

static void accept_handler(event *ev)
{
    int                 connfd;
    struct sockaddr_in  *addr;
    socklen_t           len;
    connection          *conn;
    int                 err;

    conn = conn_pool_get();
    if (conn == NULL) {
        error_log("worker %d not enough free connections", msg.worker_id);
        return;
    }

    addr = &conn->addr;
    len = sizeof(*addr);

    inter:
    connfd = accept4(ev->conn->fd, addr, &len, SOCK_NONBLOCK);
    if (connfd == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                conn_pool_free(conn);
                return;
            default:
                error_log("worker %d accept4 error: %s", msg.worker_id, strerror(errno));
                exit(1);
        }
    }

    access_log(addr, "worker %d new connection", msg.worker_id);

    conn->fd = connfd;
    conn->read->handler = read_handler;
    conn->write->handler = empty_handler;

    err = event_conn_add(conn);
    if (err == -1) {
        error_log("worker %d event_conn_add error", msg.worker_id);
        exit(1);
    }

    timer_add(conn->read, request_timeout);

    ++msg.total_connection;
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

        access_log(&conn->addr, "worker %d timeout", msg.worker_id);
        close_connection(conn);
        return;
    }

    /* 若是第一次调用则需要创建request */
    if (rqst == NULL) {

        rqst = conn->app = request_create(conn);
        if (rqst == NULL) {
            error_log("worker %d request_create error", msg.worker_id);
            exit(1);
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
                    timer_add(ev, request_timeout);
                }
                return;
            }
            if (errno != ECONNRESET) {
                access_log(&conn->addr, "read error");
                exit(1);
            }

            /* fall through */
        case 0:

            /* 对端在没有发送完整请求的情况下关闭连接 */
            access_log(&conn->addr, "read %s", n == 0 ? "FIN" : "RESET");
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
            access_log(&conn->addr, "parse_request error %s", rqst->request_start);
            rqst->status_code = HTTP_R_BAD_REQUEST;

            goto error;

        case FCY_AGAIN:
            read_handler(ev);
            return;

        case FCY_OK:

            assert(buffer_empty(header_in));
            break;

        default:
            assert(0);
    }

    /* 解析完毕 */

    if (ev->timer_set) {
        timer_del(ev);
    }

    if (conn->app_count >= request_per_conn) {
        access_log(&conn->addr, "worker %d too many requests", msg.worker_id);
        rqst->keep_alive = 0;
    }

    access_log(&conn->addr, "request %s", rqst->uri);
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
                    access_log(&conn->addr, "send response head error %s", errno == EPIPE ? "EPIPE" : "ERESET");
                    close_connection(conn);
                    return;
                default:
                    access_log(&conn->addr, "write error: %s", strerror(errno));
                    exit(1);
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

                    access_log(&conn->addr, "sendfile error %s", errno == EPIPE ? "EPIPE" : "ERESET");
                    close_connection(conn);
                    return;
                default:
                    access_log(&conn->addr, "sendfile error %s", strerror(errno));
                    exit(1);
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
    int         keep_alive;

    conn = ev->conn;
    rqst = conn->app;
    keep_alive = rqst->keep_alive;

    access_log(&conn->addr, "%s %s", rqst->uri, status_code_out_str[rqst->status_code]);

    ++msg.total_request;
    if (rqst->status_code == HTTP_R_OK) {
        ++msg.ok_request;
    }

    request_destroy(rqst);

    if (!keep_alive || conn->app_count >= request_per_conn) {
        close_connection(conn);
        return;
    }

    ev->handler = empty_handler;
    conn->read->handler = read_handler;

    /* TODO: 此处到底需不需要 */
    read_handler(conn->read);
    return;
}

static void empty_handler(event *ev)
{
}

static void close_connection(connection *conn)
{
    int                 fd;

    fd = conn->fd;

    if (event_conn_del(conn) == -1) {
        access_log(&conn->addr, "event_conn_del error");
        exit(1);
    }

    if (conn->read->timer_set) {
        timer_del(conn->read);
    }

    if (close(fd) == -1) {
        access_log(&conn->addr, "close error");
        exit(1);
    }

    conn_pool_free(conn);

    access_log(&conn->addr, " %d worker close connection", msg.worker_id);
}