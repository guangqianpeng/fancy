//
// Created by frank on 17-2-16.
//

#include "http.h"
#include "base.h"
#include "log.h"
#include "Signal.h"
#include "timer.h"
#include "connection.h"
#include "request.h"
#include "upstream.h"

/* 通用的handler */
static void accept_h(event *);
static void read_request_headers_h(event *);
static void parse_request_h(event *);
static void read_request_body(event *);
static void process_request_h(event *);

/* 专门处理动态内容的 handler */
static void peer_connect_h(event *);
static void upstream_write_request_h(event *);
static void upstream_read_response_header_h(event *);
static void upstream_parse_response_h(event *);
static void upstream_read_response_body(event *);
static void write_response_all_h(event *);

static void write_response_headers_h(event *);
static void send_file_h(event *);

/* 表示一个请求处理完成，可能关闭连接，也可能keep_alive */
static void finalize_request_h(event *);

static void response_and_close(connection *conn, int status_code);
static void close_connection(connection *conn);

static int tcp_listen();

int accept_init()
{
    connection  *conn;

    conn = conn_get();
    if (conn == NULL) {
        return FCY_ERROR;
    }

    conn->sockfd = tcp_listen();
    conn_enable_accept(conn, accept_h);

    return FCY_OK;
}

static void accept_h(event *ev)
{
    int                 connfd;
    struct sockaddr_in  *addr;
    socklen_t           len;
    connection          *conn;

    conn = conn_get();
    if (conn == NULL) {
        LOG_WARN("not enough idle connections, current %d", worker_connections);
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
                LOG_SYSERR("accept4 error");
                return;
        }
    }

    conn->sockfd = connfd;

    conn_enable_read(conn, read_request_headers_h);

    timer_add(&conn->read, (timer_msec)request_timeout);

    LOG_DEBUG("%s [up]", conn_str(conn));

    /* defer option is set,
     * so there should have data
     * */
    read_request_headers_h(&conn->read);
}

static void response_and_close(connection *conn, int status_code)
{
    request *rqst;

    assert(status_code != STATUS_OK);

    rqst = conn->app;
    rqst->should_keep_alive = 0;
    rqst->status_code = status_code;
    conn_enable_write(conn, write_response_headers_h);
    write_response_headers_h(&conn->write);
    return;
}

static void read_request_headers_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    buffer      *header_in;

    conn = ev->conn;
    rqst = conn->app;

    /* 对端发送请求超时 */
    if (ev->timeout) {
        LOG_WARN("%s request timeout (%dms)",
                 conn_str(conn), request_timeout);
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_TIME_OUT);
        return;
    }

    /* 若是第一次调用则需要创建request */
    if (rqst == NULL) {
        rqst = request_create(conn);
        if (rqst == NULL) {
            LOG_FATAL("request create failed, run out of memory");
        }
    }

    header_in = rqst->header_in;

    /* 读http request header */
    CONN_READ(conn, header_in, close_connection(conn));

    /* 解析请求 */
    parse_request_h(ev);
    return;
}

static void parse_request_h(event *ev)
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
            LOG_INFO("%s request parse error, %s",
                     conn_str(conn), rqst->header_in->start);
            conn_disable_read(conn);
            response_and_close(conn, STATUS_BAD_REQUEST);
            return;

        case FCY_AGAIN:
            /* read buffer满 */
            if (buffer_full(in)) {
                LOG_INFO("%s request too long %s",
                         conn_str(conn), rqst->header_in->start);
                conn_disable_read(conn);
                response_and_close(conn, STATUS_REQUEST_HEADER_FIELD_TOO_LARGE);
            }
            return;

        default:
            break;
    }

    /* content 过长 */
    if (rqst->content_length > HTTP_MAX_CONTENT_LENGTH) {
        LOG_INFO("%s content-length too long, %d bytes",
                 conn_str(conn), rqst->content_length);
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_HEADER_FIELD_TOO_LARGE);
        return;
    }

    /* 初始化request body相关的内容 */
    if (rqst->content_length > 0) {
        err = request_create_body_in(rqst);
        if (err == FCY_ERROR) {
            LOG_FATAL("request create body failed, run out of memory");
        }
    }

    conn->read.handler = read_request_body;
    read_request_body(ev);
    return;
}

static void read_request_body(event *ev)
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
        LOG_WARN("%s request body timeout", conn_str(conn));
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_TIME_OUT);
        return;
    }

    /* 读http request body */
    CONN_READ(conn, body_in, close_connection(conn));

    /* 整个http请求解析和读取完毕，包括body */
done:
    conn_disable_read(conn);

    if (ev->timer_set) {
        timer_del(ev);
    }

    if (!rqst->is_static) {
        /* 复原header_in, 发送给上游 */
        header_in->data_end = header_in->data_start;
        header_in->data_start = header_in->start;
    }

    if (conn->app_count >= keep_alive_requests) {
        LOG_WARN("%s too many requests", conn_str(conn));
        rqst->should_keep_alive = 0;
    }

    process_request_h(ev);

    return;
}

static void process_request_h(event *ev)
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
        response_and_close(conn, rqst->status_code);
        return;
    }

    /* 静态类型请求 */
    if (rqst->is_static) {
        err = open_static_file(rqst);
        if (err == FCY_ERROR) {
            LOG_INFO("%s open static failed", conn_str(conn));
            response_and_close(conn, rqst->status_code);
            return;
        }

        LOG_DEBUG("%s request \"%s\" %ld bytes",
                  conn_str(conn), rqst->host_uri, rqst->sbuf.st_size);

        conn_enable_write(conn, write_response_headers_h);
        write_response_headers_h(&conn->write);
        return;
    }
    else if (rqst->status_code == STATUS_OK) {
        /* 动态类型请求
         * 不支持keep-alive
         * */
        LOG_DEBUG("%s upstream request \"%.*s\"",
                  conn_str(conn),
                  rqst->parser.uri_end - rqst->parser.uri_start,
                  rqst->parser.uri_start);

        rqst->should_keep_alive = 0;
        set_conn_header_closed(rqst);
        peer_connect_h(&peer->write);
        return;
    }
    else {
        response_and_close(conn, rqst->status_code);
        return;
    }
}

static void peer_connect_h(event *ev)
{
    connection      *conn;
    peer_connection *peer;
    int             err;

    peer = ev->conn;
    conn = peer->peer;

    assert(peer->sockfd == -1);
    peer->sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (peer->sockfd == -1) {
        LOG_SYSERR("create socket error");
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    inter:
    err = connect(peer->sockfd, &peer->addr, sizeof(peer->addr));
    if (err == -1) {
        switch (errno) {
            case EINTR:
                goto inter;

            // common case
            case EINPROGRESS:
                conn_enable_write(peer, upstream_write_request_h);
                timer_add(&peer->write, (timer_msec)upstream_timeout);
                return;

            default:
                /* host unreachable, timeout, etc. */
                LOG_SYSERR("connect error");
                response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
                return;
        }
    }

    /* connect success immediately, no timer needed */
    conn_enable_write(peer, upstream_write_request_h);
    upstream_write_request_h(&peer->write);
    return;
}

static void upstream_write_request_h(event *ev)
{
    connection      *conn;
    peer_connection *peer;
    request         *rqst;
    upstream        *upstm;
    buffer          *header_in;
    buffer          *body_in;

    int conn_err;

    socklen_t err_len = sizeof(int);

    peer = ev->conn;
    conn = peer->peer;
    rqst = conn->app;
    upstm = peer->app;

    /* peer connect 超时 */
    if (ev->timeout) {
        LOG_WARN("%s upstream connect timeout", conn_str(conn));
        conn_disable_write(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    if (upstm == NULL) {

        /* 若是第一次调用:
         *   1. 检查connect调用是否成功
         *   2. 若成功则创建upstream
         *   3. 关闭connect超时
         * */

        CHECK(getsockopt(peer->sockfd, SOL_SOCKET, SO_ERROR, &conn_err, &err_len));
        if (conn_err != 0) {
            LOG_ERROR("%s upstream connect error: %s", conn_str(conn), strerror(conn_err));
            conn_disable_write(peer);
            response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
            return;
        }

        /* connect success */
        upstm = upstream_create(peer, rqst->pool);
        if (upstm == NULL) {
            LOG_FATAL("upstream create error");
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
    CONN_WRITE(peer, header_in,
              close_connection(conn));
    if (body_in != NULL) {
        CONN_WRITE(peer, body_in,
                  close_connection(conn));
    }

    /* 打开读超时 */
    assert(!peer->write.timer_set);
    timer_add(&peer->read, (timer_msec)upstream_timeout);

    conn_disable_write(peer);
    conn_enable_read(peer, upstream_read_response_header_h);

    upstream_read_response_header_h(&peer->read);
    return;
}

static void upstream_read_response_header_h(event *ev)
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
        LOG_WARN("%s upstream response timeout (%dms)",
                 conn_str(conn), upstream_timeout);
        conn_disable_read(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    /* 读 upstream http response */
    CONN_READ(peer, in, close_connection(conn));

    upstream_parse_response_h(ev);
    return;
}

static void upstream_parse_response_h(event *ev)
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
            LOG_WARN("%s parse upstream response error %s",
                     conn_str(conn), rqst->header_out->data_start);
            conn_disable_read(peer);
            response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);

        case FCY_AGAIN:
            /* read buffer满 */
            if (out->data_end == out->end) {
                conn_disable_read(peer);
                response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
            }
            return;

        default:
            break;
    }

    if (upstm->content_length > HTTP_MAX_CONTENT_LENGTH) {
        conn_disable_read(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        LOG_WARN("%s upstream content length too long, %d bytes",
                 conn_str(conn), upstm->content_length);
        return;
    }

    if (upstm->content_length > 0) {
        err = request_create_body_out(rqst, (size_t)upstm->content_length);
        if (err == FCY_ERROR) {
            LOG_FATAL("%s request create body failed, run out of memory",
                      conn_str(conn));
        }
    }

    peer->read.handler = upstream_read_response_body;
    upstream_read_response_body(ev);
    return;
}

static void upstream_read_response_body(event *ev)
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
        LOG_WARN("%s upstream response timeout", conn_str(conn));
        conn_disable_read(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    /* FIXME: internal server error  */
    CONN_READ(peer, body_out, close_connection(conn));

    done:
    if (ev->timer_set) {
        timer_del(ev);
    }

    /* 复原header_out, 发送给下游 */
    header_out->data_end = header_out->data_start;
    header_out->data_start = header_out->start;

    conn_disable_read(peer);
    conn_enable_write(conn, write_response_all_h);
    write_response_all_h(&conn->write);
    return;
}

static void write_response_all_h(event *ev)
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
    CONN_WRITE(conn, header_out,
              close_connection(conn));
    if (body_out != NULL) {
        CONN_WRITE(conn, body_out,
                  close_connection(conn));
    }

    conn_disable_write(conn);
    finalize_request_h(ev);
    return;
}

static void write_response_headers_h(event *ev)
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

        if (rqst->status_code == STATUS_OK) {
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

        if (rqst->status_code != STATUS_OK) {
            buffer_write(header_out, status_str, strlen(status_str));
        }
    }

    CONN_WRITE(conn, header_out,
              close_connection(conn));

    if (rqst->send_fd > 0) {
        ev->handler = send_file_h;
    } else {
        conn_disable_write(conn);
        ev->handler = finalize_request_h;
    }

    ev->handler(ev);
    return;
}

static void send_file_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    struct stat *sbuf;
    int         send_fd;

    conn = ev->conn;
    rqst = conn->app;
    sbuf = &rqst->sbuf;
    send_fd = rqst->send_fd;

    CONN_SEND_FILE(conn, send_fd, sbuf,
                  close_connection(conn));

    conn_disable_write(conn);
    finalize_request_h(ev);
    return;
}

static void finalize_request_h(event *ev)
{
    connection  *conn;
    request     *rqst;
    int         keep_alive;

    conn = ev->conn;
    rqst = conn->app;
    keep_alive = rqst->should_keep_alive;

    LOG_DEBUG("%s response \"%s\"",
             conn_str(conn), status_code_out_str[rqst->status_code]);

    if (!keep_alive || conn->app_count >= keep_alive_requests) {
        close_connection(conn);
        return;
    }

    conn_enable_read(conn, read_request_headers_h);

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
        CHECK(close(peer->sockfd));
    }

    /* close connection */
    if (conn->app) {
        request_destroy(conn->app);
    }
    if (conn->read.timer_set) {
        timer_del(&conn->read);
    }
    /* epoll will automaticly remove fd */
    CHECK(close(conn->sockfd));

    conn_free(conn);

    LOG_DEBUG("%s [down]", conn_str(conn));
}

static int tcp_listen()
{
    int                 listenfd;
    struct sockaddr_in  servaddr;
    socklen_t           addrlen;
    const int           on = 1;
    int                 err;

    listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenfd == -1) {
        return FCY_ERROR;
    }

    CHECK(setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)));

    CHECK(setsockopt(listenfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &accept_defer, sizeof(accept_defer)));

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    servaddr.sin_port           = htons((uint16_t)listen_on);

    addrlen = sizeof(servaddr);
    err = bind(listenfd, (struct sockaddr*)&servaddr, addrlen);
    if (err == -1) {
        LOG_SYSERR("bind failed");
        return FCY_ERROR;
    }

    err = listen(listenfd, 1024);
    if (err == -1) {
        LOG_SYSERR("listen failed");
        return FCY_ERROR;
    }

    return listenfd;
}