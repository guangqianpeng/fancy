//
// Created by frank on 17-2-16.
//

#include "http.h"
#include "base.h"
#include "Signal.h"
#include "timer.h"
#include "connection.h"
#include "request.h"
#include "upstream.h"
#include "app.h"

#define CHECK_DISABLE_READ(conn) \
ABORT_ON(conn_disable_read(conn), FCY_ERROR);

#define CHECK_ENABLE_READ(conn, handler, flag) \
ABORT_ON(conn_enable_read(conn, handler, flag), FCY_ERROR);

#define CHECK_DISABLE_WRITE(conn) \
ABORT_ON(conn_disable_write(conn), FCY_ERROR)

#define CHECK_ENABLE_WRITE(conn, handler, flag) \
ABORT_ON(conn_enable_write(conn, handler, flag), FCY_ERROR)

#define DISABLE_READ_AND_RESPONSE(conn, status_code) \
do {    \
    CHECK_DISABLE_READ(conn);    \
    response_and_close_on(conn, status_code);   \
} while(0)


typedef void(*conn_handler)(connection*);

static int localfd[2];

static struct {
    int worker_id;
    int total_connection;
    int total_request;
    int ok_request;
} msg, total;

static void sig_empty_handler(int signo);
static void sig_quit_handler(int signo);

static void response_and_close_on(connection *conn, int status_code);
static void close_connection(connection *conn);

int main()
{
    int     err;

    /* 单进程模式 */
    if (single_process) {

        err = init_worker(accept_h);
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

                err = init_worker(accept_h);
                if (err == FCY_ERROR) {
                    error_log("worker %d init worker error", i);
                    exit(1);
                }

                error_log("worker %d listening port %d", i, serv_port);

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

static void sig_quit_handler(int signo)
{
    ssize_t err;

    err = write(localfd[1], &msg, sizeof(msg));
    if (err != sizeof(msg)) {
        err = write(STDERR_FILENO, "worker write failed\n", 20);
        (void)err;
    }

    exit(0);
}

static void response_and_close_on(connection *conn, int status_code)
{
    request *rqst;
    int     err;

    assert(status_code != HTTP_STATUS_OK);

    rqst = conn->app;
    rqst->should_keep_alive = 0;
    rqst->status_code = status_code;
    err = conn_enable_write(conn,
                            write_response_headers_h,
                            EPOLLET);
    ABORT_ON(err, -1);
    write_response_headers_h(&conn->write);
    return;
}

static void sig_empty_handler(int signo)
{
}

void accept_h(event *ev)
{
    int                 connfd;
    struct sockaddr_in  *addr;
    socklen_t           len;
    connection          *conn;

    conn = conn_get();
    if (conn == NULL) {
        error_log("worker %d not enough free connections", msg.worker_id);
        return;
    }

    addr = &conn->addr;
    len = sizeof(*addr);

    inter:
    connfd = accept4(ev->conn->sockfd, addr, &len, SOCK_NONBLOCK);
    if (connfd == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                conn_free(conn);
                return;
            default:
                error_log("worker %d accept4 error: %s", msg.worker_id, strerror(errno));
                exit(1);
        }
    }

    access_log(addr, "worker %d new connection", msg.worker_id);

    conn->sockfd = connfd;
    ABORT_ON(conn_enable_read(conn, read_request_headers_h, EPOLLET), FCY_ERROR);

    timer_add(&conn->read, (timer_msec)request_timeout);

    ++msg.total_connection;

    /* defer option is set,
     * so there should have data
     * */
    read_request_headers_h(&conn->read);
}

void read_request_headers_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_in;

    conn = ev->conn;
    rqst = conn->app;

    /* 对端发送请求超时 */
    if (ev->timeout) {
        access_log(&conn->addr, "worker %d timeout", msg.worker_id);
        DISABLE_READ_AND_RESPONSE(conn, HTTP_STATUS_REQUEST_TIME_OUT);
        return;
    }

    /* 若是第一次调用则需要创建request */
    if (rqst == NULL) {
        rqst = request_create(conn);
        if (rqst == NULL) {
            error_log("worker %d request_create error", msg.worker_id);
            exit(1);
        }
    }

    header_in = rqst->header_in;

    /* 读http request header */
    FCY_READ(conn, header_in, close_connection(conn));

    /* 解析请求 */
    parse_request_h(ev);
    return;
}

void parse_request_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *in;
    int         err;

    conn = ev->conn;
    rqst = conn->app;
    in = rqst->header_in;

    /* 解析请求 */
    err = request_parse(rqst);
    switch (err) {

        case FCY_ERROR:
            access_log(&conn->addr, "parse_request error %s", rqst->request_start);
            DISABLE_READ_AND_RESPONSE(conn, HTTP_STATUS_BAD_REQUEST);
            return;

        case FCY_AGAIN:
            /* read buffer满 */
            if (in->data_end == in->end) {
                DISABLE_READ_AND_RESPONSE(conn,
                                          HTTP_STATUS_REQUEST_HEADER_FIELD_TOO_LARGE);
                return;
            }
            return;

        default:
            break;
    }

    /* content 过长 */
    if (rqst->content_length > HTTP_MAX_CONTENT_LENGTH) {
        DISABLE_READ_AND_RESPONSE(conn, HTTP_STATUS_PAYLOAD_TOO_LARGE);
        return;
    }

    /* 初始化request body相关的内容 */
    if (rqst->content_length > 0) {
        err = request_create_body_in(rqst);
        ABORT_ON(err, FCY_ERROR);
    }

    conn->read.handler = read_request_body;
    read_request_body(ev);
    return;
}

void read_request_body(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_in;
    buffer      *body_in;

    conn = ev->conn;
    rqst = conn->app;
    header_in = rqst->header_in;
    body_in = rqst->body_in;

    /* no request body */
    if (rqst->content_length == 0) {
        goto done;
    }

    /* already read done */
    if (buffer_full(body_in)) {
        goto done;
    }

    /* 对端发送body超时 */
    if (ev->timeout) {
        access_log(&conn->addr, "worker %d timeout", msg.worker_id);
        DISABLE_READ_AND_RESPONSE(conn, HTTP_STATUS_REQUEST_TIME_OUT);
        return;
    }

    /* 读http request body */
    FCY_READ(conn, body_in, close_connection(conn));

    /* 整个http请求解析和读取完毕，包括body */
done:
    CHECK_DISABLE_READ(conn);

    if (ev->timer_set) {
        timer_del(ev);
    }

    if (!rqst->is_static) {
        /* 复原header_in, 发送给上游 */
        header_in->data_end = header_in->data_start;
        header_in->data_start = header_in->start;
    }

    if (conn->app_count >= request_per_conn) {
        access_log(&conn->addr, "worker %d too many requests", msg.worker_id);
        rqst->should_keep_alive = 0;
    }

    access_log(&conn->addr, "request %s", rqst->host_uri);

    process_request_h(ev);

    return;
}

void process_request_h(event *ev)
{
    connection  *conn;
    connection  *peer;
    request     *rqst;
    int         err;

    conn = ev->conn;
    peer = conn->peer;
    rqst = conn->app;

    err = check_request_header(rqst);
    if (err == FCY_ERROR) {
        response_and_close_on(conn, rqst->status_code);
        return;
    }

    /* 静态类型请求 */
    if (rqst->is_static) {
        err = open_static_file(rqst);
        if (err == FCY_ERROR) {
            response_and_close_on(conn, rqst->status_code);
            return;
        }

        ABORT_ON(conn_enable_write(conn,
                                   write_response_headers_h,
                                   EPOLLET), FCY_ERROR);
        write_response_headers_h(&conn->write);
        return;
    }
    else if (use_upstream) {
        /* 动态类型请求
         * 不支持keep-alive
         * */
        rqst->should_keep_alive = 0;
        set_conn_header_closed(rqst);
        peer_connect_h(&peer->write);
        return;
    }
    else {
        response_and_close_on(conn, HTTP_STATUS_BAD_REQUEST);
        return;
    }
}

void peer_connect_h(event *ev)
{
    connection      *conn;
    peer_connection *peer;
    int             err;

    peer = ev->conn;
    conn = peer->peer;

    assert(peer->sockfd == -1);
    peer->sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ABORT_ON(peer->sockfd, -1);

    inter:
    err = connect(peer->sockfd, &peer->addr, sizeof(peer->addr));
    if (err == -1) {
        switch (errno) {
            case EINTR:
                goto inter;

            // common case
            case EINPROGRESS:
                err = conn_enable_write(peer,
                                        upstream_write_request_h,
                                        EPOLLET);
                ABORT_ON(err, FCY_ERROR);
                timer_add(&peer->write, (timer_msec)upstream_timeout);
                return;

            default:
                /* host unreachable, timeout, etc. */
                error_log("connect error");
                response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
                return;
        }
    }

    /* connect success immediately, no timer needed */
    err = conn_enable_write(peer,
                            upstream_write_request_h,
                            EPOLLET);
    ABORT_ON(err, FCY_ERROR);
    upstream_write_request_h(&peer->write);
    return;
}

void upstream_write_request_h(event *ev)
{
    connection      *conn;
    peer_connection *peer;
    request         *rqst;
    upstream        *upstm;
    buffer          *header_in;
    buffer          *body_in;

    int err, conn_err;

    socklen_t err_len = sizeof(int);

    peer = ev->conn;
    conn = peer->peer;
    rqst = conn->app;
    upstm = peer->app;

    /* peer connect 超时 */
    if (ev->timeout) {
        access_log(&peer->addr, "upstream connect timeout");
        CHECK_DISABLE_WRITE(peer);
        response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    if (upstm == NULL) {

        /* 若是第一次调用:
         *   1. 检查connect调用是否成功
         *   2. 若成功则创建upstream
         *   3. 关闭connect超时
         * */

        err = getsockopt(peer->sockfd, SOL_SOCKET, SO_ERROR, &conn_err, &err_len);
        ABORT_ON(err, -1);
        if (conn_err != 0) {
            error_log("peer connect error: %s", strerror(conn_err));
            CHECK_DISABLE_WRITE(peer);
            response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
            return;
        }

        /* connect success */
        upstm = upstream_create(peer, rqst->pool);
        if (upstm == NULL) {
            error_log("worker %d upstream_create error", msg.worker_id);
            exit(1);
        }

        /* del timer */
        if (ev->timer_set) {
            timer_del(ev);
        }
    }

    /* write request buffer to upstream */
    header_in = rqst->header_in;
    body_in = rqst->body_in;

    /* TODO: use writev instead */
    FCY_WRITE(peer, header_in,
              close_connection(conn));
    if (body_in != NULL) {
        FCY_WRITE(peer, body_in,
                  close_connection(conn));
    }

    /* 打开读超时 */
    assert(!peer->write.timer_set);
    timer_add(&peer->read, (timer_msec)upstream_timeout);

    CHECK_DISABLE_WRITE(peer);
    err = conn_enable_read(peer, upstream_read_response_header_h, EPOLLET);
    ABORT_ON(err, FCY_ERROR);

    upstream_read_response_header_h(&peer->read);
    return;
}

void upstream_read_response_header_h(event *ev)
{
    connection  *conn;
    connection  *peer;
    request     *rqst;
    buffer      *in;

    peer = ev->conn;
    conn = peer->peer;
    rqst = conn->app;
    in = rqst->header_out;

    if (ev->timeout) {
        access_log(&conn->addr, "upstream response timeout", msg.worker_id);
        CHECK_DISABLE_READ(peer);
        response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    /* 读 upstream http response */
    FCY_READ(peer, in, close_connection(conn));

    upstream_parse_response_h(ev);
    return;
}

void upstream_parse_response_h(event *ev)
{
    connection  *conn;
    connection  *peer;
    upstream    *upstm;
    request     *rqst;
    buffer      *out;
    int         err;

    peer = ev->conn;
    conn = peer->peer;
    upstm = peer->app;
    rqst = conn->app;
    out = rqst->header_out;

    err = upstream_parse(upstm, rqst->header_out);
    switch (err) {
        case FCY_ERROR:
            access_log(&conn->addr,
                       "parse response error %s",
                       rqst->header_out->data_start);
            CHECK_DISABLE_READ(peer);
            response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);

        case FCY_AGAIN:
            /* read buffer满 */
            if (out->data_end == out->end) {
                CHECK_DISABLE_READ(peer);
                response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
            }
            return;

        default:
            break;
    }

    if (upstm->content_length > HTTP_MAX_CONTENT_LENGTH) {
        CHECK_DISABLE_READ(peer);
        response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    if (upstm->content_length > 0) {
        err = request_create_body_out(rqst, (size_t)upstm->content_length);
        ABORT_ON(err, FCY_ERROR);
    }

    peer->read.handler = upstream_read_response_body;
    upstream_read_response_body(ev);
    return;
}

void upstream_read_response_body(event *ev)
{
    connection  *conn;
    connection  *peer;
    upstream    *upstm;
    request     *rqst;
    buffer      *header_out;
    buffer      *body_out;

    peer = ev->conn;
    conn = peer->peer;
    upstm = peer->app;
    rqst = conn->app;
    header_out = rqst->header_out;
    body_out = rqst->body_out;

    if (upstm->content_length == 0) {
        goto done;
    }

    if (buffer_full(body_out)) {
        goto done;
    }

    if (ev->timeout) {
        access_log(&conn->addr, "upstream response timeout", msg.worker_id);
        CHECK_DISABLE_READ(peer);
        response_and_close_on(conn, HTTP_STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    /* FIXME: internal server error  */
    FCY_READ(peer, body_out, close_connection(conn));

    done:
    if (ev->timer_set) {
        timer_del(ev);
    }

    /* 复原header_out, 发送给下游 */
    header_out->data_end = header_out->data_start;
    header_out->data_start = header_out->start;

    CHECK_DISABLE_READ(peer);
    CHECK_ENABLE_WRITE(conn, write_response_all_h, EPOLLET);
    write_response_all_h(&conn->write);
    return;
}

void write_response_all_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_out;
    buffer      *body_out;

    conn = ev->conn;
    rqst = conn->app;
    header_out = rqst->header_out;
    body_out = rqst->body_out;

    /* TODO: use writev instead */
    FCY_WRITE(conn, header_out,
              close_connection(conn));
    if (body_out != NULL) {
        FCY_WRITE(conn, body_out,
                  close_connection(conn));
    }

    CHECK_DISABLE_WRITE(conn);
    finalize_request_h(ev);
    return;
}

void write_response_headers_h(event *ev)
{
    connection *conn;
    request *rqst;
    buffer *header_out;
    const char *status_str;
    int n;

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

        if (rqst->status_code == HTTP_STATUS_OK) {
            buffer_write(header_out, rqst->content_type,
                         strlen(rqst->content_type));
            buffer_write(header_out, "\r\nContent-Length: ", 18);
            n = sprintf((char *) header_out->data_end, "%ld", rqst->sbuf.st_size);
            buffer_seek_end(header_out, (int) n);
        } else {
            buffer_write(header_out, "text/html; charset=utf-8", 24);
            buffer_write(header_out, "\r\nContent-Length: ", 18);
            n = sprintf((char *) header_out->data_end, "%ld", strlen(status_str));
            buffer_seek_end(header_out, (int) n);
        }

        if (rqst->should_keep_alive) {
            buffer_write(header_out, "\r\nConnection: keep-alive\r\n\r\n", 28);
        } else {
            buffer_write(header_out, "\r\nConnection: close\r\n\r\n", 23);
        }

        if (rqst->status_code != HTTP_STATUS_OK) {
            buffer_write(header_out, status_str, strlen(status_str));
        }
    }

    FCY_WRITE(conn, header_out,
              close_connection(conn));

    if (rqst->send_fd > 0) {
        ev->handler = send_file_h;
    } else {
        CHECK_DISABLE_WRITE(conn);
        ev->handler = finalize_request_h;
    }

    ev->handler(ev);
    return;
}

void send_file_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    struct stat *sbuf;
    int         send_fd;

    conn = ev->conn;
    rqst = conn->app;
    sbuf = &rqst->sbuf;
    send_fd = rqst->send_fd;

    FCY_SEND_FILE(conn, send_fd, sbuf,
                  close_connection(conn));

    CHECK_DISABLE_WRITE(conn);
    finalize_request_h(ev);
    return;
}

void finalize_request_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    int         keep_alive;

    conn = ev->conn;
    rqst = conn->app;
    keep_alive = rqst->should_keep_alive;

    access_log(&conn->addr, "%s %s", rqst->host_uri, status_code_out_str[rqst->status_code]);

    ++msg.total_request;
    if (rqst->status_code == HTTP_STATUS_OK) {
        ++msg.ok_request;
    }

    if (!keep_alive || conn->app_count >= request_per_conn) {
        close_connection(conn);
        return;
    }

    ABORT_ON(conn_enable_read(conn,
                              read_request_headers_h,
                              EPOLLET), FCY_ERROR);

    timer_add(&conn->read, (timer_msec)request_timeout);

    assert(buffer_empty(rqst->header_in));
    request_reset(rqst);
}

static void close_connection(connection *conn)
{
    /* close peer connection first */
    connection *peer = conn->peer;

    if (peer->app) {
        upstream_destroy(peer->app);
    }
    if (peer->read.timer_set) {
        timer_del(&peer->read);
    }
    if (peer->write.timer_set) {
        timer_del(&peer->write);
    }
    if (peer->sockfd >= 0) {
        ABORT_ON(close(peer->sockfd), -1);
    }

    /* close connection */
    if (conn->app) {
        request_destroy(conn->app);
    }
    if (conn->read.timer_set) {
        timer_del(&conn->read);
    }
    /* epoll will automaticly remove fd */
    ABORT_ON(close(conn->sockfd), -1);

    conn_free(conn);

    access_log(&conn->addr, "worker %d close connection", msg.worker_id);
}