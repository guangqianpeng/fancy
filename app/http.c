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

static int Read(connection *conn, void *data, int *n);
static int Write(connection *conn, const void *data, int *n);

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

    assert(status_code != HTTP_R_OK);

    rqst = conn->app;
    rqst->keep_alive = 0;
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

static int Read(connection *conn, void *data, int *n)
{
    int ret;

    eintr:
    ret = (int)read(conn->sockfd, data, (size_t)(*n));
    switch (ret) {
        case -1:
            if (errno == EINTR) {
                goto eintr;
            }
            if (errno == EAGAIN) {
                return FCY_AGAIN;
            }
            if (errno != ECONNRESET) {
                access_log(&conn->addr, "read error");
                exit(1);
            }

            /* fall through */
        case 0:

            /* 对端在没有发送完整请求的情况下关闭连接 */
            access_log(&conn->addr, "read %s", ret == 0 ? "FIN" : "RESET");
            return FCY_ERROR;

        default:
            break;
    }
    *n = ret;
    return FCY_OK;
}

static int Write(connection *conn, const void *data, int *n)
{
    int ret;

    inter:
    ret = (int)write(conn->sockfd, data, (size_t)*n);
    if (ret == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                return FCY_AGAIN;
            case EPIPE:
            case ECONNRESET:
                access_log(&conn->addr, "write error %s", errno == EPIPE ? "EPIPE" : "ERESET");
                return FCY_ERROR;
            default:
                access_log(&conn->addr, "write error: %s", strerror(errno));
                exit(1);
        }
    }
    *n = ret;
    return FCY_OK;
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
    assert(connfd != -1);
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

    /* FIXME: should read handler here? */
    read_request_headers_h(&conn->read);
}

void read_request_headers_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_in;
    int         err, n;

    conn = ev->conn;
    rqst = conn->app;

    /* 对端发送请求超时 */
    if (ev->timeout) {
        access_log(&conn->addr, "worker %d timeout", msg.worker_id);
        close_connection(conn);
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

    /* 读http request header */
    header_in = rqst->header_in;
    n = (int)buffer_space(header_in);
    err = Read(conn, header_in->data_end, &n);
    switch (err) {
        case FCY_ERROR:
            close_connection(conn);
            return;
        case FCY_AGAIN:
            return;
        default:
            buffer_seek_end(header_in, n);
            break;
    }

    /* read buffer满 */
    if (buffer_full(header_in)) {
        ABORT_ON(conn_disable_read(conn), FCY_ERROR);
        response_and_close_on(conn, HTTP_R_URI_TOO_LONG);
        return;
    }

    /* 解析请求 */
    parse_request_h(ev);
    return;
}

void parse_request_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    int         err;

    conn = ev->conn;
    rqst = conn->app;

    /* 解析请求 */
    err = parse_request(rqst);
    switch (err) {

        case FCY_ERROR:
            access_log(&conn->addr, "parse_request error %s", rqst->request_start);
            ABORT_ON(conn_disable_read(conn), FCY_ERROR);
            response_and_close_on(conn, HTTP_R_BAD_REQUEST);
            return;

        case FCY_AGAIN:
            return;

        case FCY_OK:
            break;

        default:
            /* never reach here */
            abort();
    }

    /* 可能已经读取了部分body */
    long cnt_len = rqst->content_length;
    long buf_len = buffer_size(rqst->header_in);

    /* content 过长 */
    if (cnt_len < buf_len) {
        ABORT_ON(conn_disable_read(conn), FCY_ERROR);
        response_and_close_on(conn, HTTP_R_PAYLOAD_TOO_LARGE);
        return;
    }

    rqst->rest_content_length = cnt_len - buf_len;
    conn->read.handler = read_request_body;
    read_request_body(ev);
    return;
}

void read_request_body(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_in;
    int         err, n;

    conn = ev->conn;
    rqst = conn->app;

    if (rqst->has_content_length_header
        || rqst->rest_content_length == 0) {
        goto done;
    }

    /* 对端发送body超时 */
    if (ev->timeout) {
        access_log(&conn->addr, "worker %d timeout", msg.worker_id);
        close_connection(conn);
        return;
    }

    /* 读http request body */
    header_in = rqst->header_in;
    n = (int)buffer_space(header_in);
    err = Read(conn, header_in->data_end, &n);
    switch (err) {
        case FCY_ERROR:
            close_connection(conn);
            return;
        case FCY_AGAIN:
            return;
        default:
            buffer_seek_end(header_in, n);
            break;
    }

    if (buffer_full(header_in)) {
        ABORT_ON(conn_disable_read(conn), FCY_ERROR);
        response_and_close_on(conn, HTTP_R_PAYLOAD_TOO_LARGE);
        return;
    }

    /* 整个http请求解析和读取完毕，包括body */
done:
    ABORT_ON(conn_disable_read(conn), FCY_ERROR);

    if (ev->timer_set) {
        timer_del(ev);
    }

    if (conn->app_count >= request_per_conn) {
        access_log(&conn->addr, "worker %d too many requests", msg.worker_id);
        rqst->keep_alive = 0;
    }

    access_log(&conn->addr, "request %s", rqst->uri);

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

    err = check_request_header_field(rqst);
    if (err == FCY_ERROR) {
        response_and_close_on(conn, rqst->status_code);
        return;
    }

    /* 静态类型请求 */
    if (rqst->is_static) {
        err = init_request_static(rqst);
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
    else {
        /* 动态类型请求
         * 不支持keep-alive, 呵呵
         * */
        rqst->keep_alive = 0;
        ABORT_ON(conn_enable_write(peer,
                                   peer_connect_h,
                                   EPOLLET), FCY_ERROR);
        peer_connect_h(&peer->write);
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
                response_and_close_on(conn, HTTP_R_INTARNAL_SEARVE_ERROR);
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

    int fd, err, conn_err, n;

    socklen_t err_len = sizeof(int);

    conn = ev->conn;
    peer = conn->peer;
    rqst = conn->app;
    upstm = peer->app;

    fd = peer->sockfd;

    /* peer connect 超时 */
    if (ev->timeout) {
        ABORT_ON(conn_disable_write(peer), FCY_ERROR);
        response_and_close_on(conn, HTTP_R_INTARNAL_SEARVE_ERROR);
        return;
    }

    if (upstm == NULL) {

        /* 若是第一次调用:
         *   1. 检查connect调用是否成功
         *   2. 若成功则创建upstream
         *   3. 关闭connect超时
         * */

        err = getsockopt(fd, SOL_SOCKET, SO_ERROR, &conn_err, &err_len);
        ABORT_ON(err, -1);
        if (conn_err != 0) {
            error_log("connect error");
            ABORT_ON(conn_disable_write(peer), FCY_ERROR);
            response_and_close_on(conn, HTTP_R_INTARNAL_SEARVE_ERROR);
            return;
        }

        /* connect success */
        upstm = upstream_create(peer);
        if (upstm == NULL) {
            error_log("worker %d upstream_create error", msg.worker_id);
            exit(1);
        }

        /* del timer */
        if (ev->timer_set) {
            timer_del(ev);
        }
    }

    // write request buffer to upstream
    header_in = rqst->header_in;
    header_in->data_start = header_in->start;
    n = (int)buffer_size(header_in);
    err = Write(peer, header_in->start, &n);
    switch (err) {
        case FCY_ERROR:
            close_connection(conn);
            return;
        case FCY_AGAIN:
            return;
        default:
            buffer_seek_start(header_in, n);
            break;
    }
    if (!buffer_empty(header_in)) {
        return;
    }

    /* 打开读超时 */
    timer_add(&peer->read, (timer_msec)upstream_timeout);

    ABORT_ON(conn_disable_write(peer), FCY_ERROR);
    err = conn_enable_read(peer, upstream_read_response_h, EPOLLET);
    ABORT_ON(err, FCY_ERROR);

    upstream_read_response_h(&peer->read);
    return;
}

void upstream_read_response_h(event *ev)
{
    connection  *conn;
    connection  *peer;

    peer = ev->conn;
    conn = peer->peer;

    // TODO read upstream response
    response_and_close_on(conn, HTTP_R_INTARNAL_SEARVE_ERROR);
}

void upstream_write_response_h(event *ev)
{
}

void write_response_headers_h(event *ev)
{
    connection *conn;
    request *rqst;
    buffer *header_out;
    const char *status_str;
    int n, err;

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
        } else {
            buffer_write(header_out, "text/html; charset=utf-8", 24);
            buffer_write(header_out, "\r\nContent-Length: ", 18);
            n = sprintf((char *) header_out->data_end, "%ld", strlen(status_str));
            buffer_seek_end(header_out, (int) n);
        }

        if (rqst->keep_alive) {
            buffer_write(header_out, "\r\nConnection: keep-alive\r\n\r\n", 28);
        } else {
            buffer_write(header_out, "\r\nConnection: close\r\n\r\n", 23);
        }

        if (rqst->status_code != HTTP_R_OK) {
            buffer_write(header_out, status_str, strlen(status_str));
        }
    }

    n = (int)buffer_size(header_out);
    err = Write(conn, header_out->data_start, &n);
    switch (err) {
        case FCY_ERROR:
            close_connection(conn);
            break;
        case FCY_AGAIN:
            return;
        default:
            break;
    }

    buffer_seek_start(header_out, n);
    if (!buffer_empty(header_out)) {
        return;
    }

    if (rqst->send_fd > 0) {
        ev->handler = send_file_h;
    } else {
        ABORT_ON(conn_disable_write(conn), FCY_ERROR);
        ev->handler = finalize_request_h;
    }

    ev->handler(ev);
    return;
}

void send_file_h(event *ev)
{
    connection *conn;
    request *rqst;
    struct stat *sbuf;
    ssize_t n;

    conn = ev->conn;
    rqst = conn->app;
    sbuf = &rqst->sbuf;

    inter:
    n = sendfile(conn->sockfd, rqst->send_fd, NULL, (size_t) sbuf->st_size);
    if (n == -1) {
        switch (errno) {
            case EINTR:
                goto inter;
            case EAGAIN:
                return;
            case EPIPE:
            case ECONNRESET:
                access_log(&conn->addr, "sendfile error %s", errno == EPIPE ? "EPIPE" : "ERESET");
                close_connection(conn);
                return;
            default:
                access_log(&conn->addr, "sendfile error %s", strerror(errno));
                exit(1);
        }
    }
    sbuf->st_size -= n;
    if (sbuf->st_size > 0) {
        return;
    }

    ABORT_ON(conn_disable_write(conn), FCY_ERROR);
    ev->handler = finalize_request_h;
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
    keep_alive = rqst->keep_alive;

    access_log(&conn->addr, "%s %s", rqst->uri, status_code_out_str[rqst->status_code]);

    ++msg.total_request;
    if (rqst->status_code == HTTP_R_OK) {
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