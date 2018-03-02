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

/* generic handler */
static void accept_h(event *);
static void read_request_headers_h(event *);
static void parse_request_h(event *);
static void read_request_body(event *);
static void process_request_h(event *);

/* dynamic content handler */
static void peer_connect_h(event *);
static void upstream_write_request_h(event *);
static void upstream_read_response_header_h(event *);
static void upstream_parse_response_h(event *);
static void upstream_read_response_body(event *);
static void write_response_all_h(event *);

static void write_response_headers_h(event *);
static void send_file_h(event *);

/* a request is finished, connection can be closed or keep alive */
static void finalize_request_h(event *);

static void response_and_close(connection *conn, int status_code);
static void close_connection(connection *conn);

static int tcp_listen();

int accept_init()
{
    connection *conn = conn_get();
    if (conn == NULL) {
        return FCY_ERROR;
    }

    conn->sockfd = tcp_listen();
    conn_enable_accept(conn, accept_h);

    return FCY_OK;
}

static void accept_h(event *ev)
{
    connection *conn = conn_get();
    if (conn == NULL) {
        LOG_WARN("not enough idle connections, current %d", worker_connections);
        return;
    }

    struct sockaddr_in  *addr = &conn->addr;
    socklen_t           len = sizeof(*addr);
    int                 connfd;

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

    /* defer option is set, we should read data */
    read_request_headers_h(&conn->read);
}

static void response_and_close(connection *conn, int status_code)
{
    assert(status_code != STATUS_OK);

    request *rqst = conn->app;

    rqst->should_keep_alive = 0;
    rqst->status_code = status_code;
    conn_enable_write(conn, write_response_headers_h);
    write_response_headers_h(&conn->write);
}

static void read_request_headers_h(event *ev)
{
    connection *conn = ev->conn;

    /* peer timeout */
    if (ev->timeout) {
        LOG_WARN("%s request timeout (%dms)",
                 conn_str(conn), request_timeout);
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_TIME_OUT);
        return;
    }

    /* create request struct if first use */
    request *rqst = conn->app;
    if (rqst == NULL) {
        rqst = request_create(conn);
        if (rqst == NULL) {
            LOG_FATAL("request create failed, run out of memory");
            exit(EXIT_FAILURE); // make compiler happy
        }
    }

    buffer *header_in = rqst->header_in;

    /* 读http request header */
    CONN_READ(conn, header_in, close_connection(conn));

    /* 解析请求 */
    parse_request_h(ev);
}

static void parse_request_h(event *ev)
{
    connection  *conn = ev->conn;
    request     *rqst = conn->app;

    int err = request_parse(rqst);
    switch (err) {

        case FCY_ERROR:
            LOG_INFO("%s request parse error, \"%s\"", conn_str(conn));
            conn_disable_read(conn);
            response_and_close(conn, STATUS_BAD_REQUEST);
            return;

        case FCY_AGAIN:
            return;

        default:
            break;
    }

    buffer_retrieve(rqst->header_in, rqst->parser.where);

    /* content too long */
    if (rqst->content_length > HTTP_MAX_CONTENT_LENGTH) {
        LOG_WARN("%s content-length too long, %d bytes",
                 conn_str(conn), rqst->content_length);
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_HEADER_FIELD_TOO_LARGE);
        return;
    }

    /* move body */
    if (!buffer_empty(rqst->header_in)) {
        buffer_transfer(rqst->body_in, rqst->header_in);
    }

    conn->read.handler = read_request_body;
    read_request_body(ev);
}

static void read_request_body(event *ev)
{
    connection  *conn = ev->conn;
    request     *rqst = conn->app;

    /* peer timeout */
    if (ev->timeout) {
        LOG_WARN("%s request body timeout", conn_str(conn));
        conn_disable_read(conn);
        response_and_close(conn, STATUS_REQUEST_TIME_OUT);
        return;
    }

    buffer *body_in = rqst->body_in;
    size_t readable;

    /* content-length */
    if (rqst->has_content_length_header) {
        readable = buffer_readable_bytes(body_in);
        if (readable >= (size_t)rqst->content_length) {
            goto done;
        }
        CONN_READ(conn, body_in, close_connection(conn));
        if (buffer_readable_bytes(body_in) < (size_t)rqst->content_length) {
            return;
        }
    }
    else if (rqst->is_chunked) {
        LOG_ERROR("CHUNKED");
        pause();
    }

    /* http request read and parse is done */
done:
    readable = buffer_readable_bytes(body_in);
    if (readable > (size_t)rqst->content_length) {
        LOG_WARN("%s read extra request body", conn_str(conn));
        /* trunc */
        buffer_unwrite(body_in, readable - rqst->content_length);
    }

    conn_disable_read(conn);

    if (ev->timer_set) {
        timer_del(ev);
    }

    if (conn->app_count >= keep_alive_requests) {
        LOG_WARN("%s too many requests", conn_str(conn));
        rqst->should_keep_alive = 0;
    }

    process_request_h(ev);
}

static void process_request_h(event *ev)
{
    connection  *conn = ev->conn;
    connection  *peer = conn->peer;
    request     *rqst = conn->app;

    int err = check_request_header(rqst);
    if (err == FCY_ERROR) {
        response_and_close(conn, rqst->status_code);
        return;
    }

    if (rqst->is_static) {
        /* static file request */

        err = open_static_file(rqst);
        if (err == FCY_ERROR) {
            LOG_INFO("%s open static failed", conn_str(conn));
            response_and_close(conn, rqst->status_code);
            return;
        }

        LOG_DEBUG("%s request \"%s\" %ld bytes",
                  conn_str(conn), rqst->uri.data, rqst->sbuf.st_size);

        conn_enable_write(conn, write_response_headers_h);
        write_response_headers_h(&conn->write);
        return;
    }
    else if (rqst->status_code == STATUS_OK) {
        /* dynamic request, proxy to upstream */

        LOG_DEBUG("%s upstream %s \"%s\"",
                  conn_str(conn), method_str[rqst->parser.method].data, rqst->uri.data);

        rqst->should_keep_alive = 0;
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
    connection      *peer = ev->conn;
    connection      *conn = peer->peer;
    int             err;

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
}

static void upstream_write_request_h(event *ev)
{
    peer_connection *peer = ev->conn;
    connection      *conn = peer->peer;
    request         *rqst = conn->app;
    upstream        *upstm = peer->app;

    /* peer connect timeout */
    if (ev->timeout) {
        LOG_WARN("%s upstream connect timeout", conn_str(conn));
        conn_disable_write(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    if (upstm == NULL) {

        /* first time send a request to upstream:
         *   1. connect success
         *   2. create upstream
         *   3. remove connect timeout
         *   4. write request header
         * */
        int         conn_err;
        socklen_t   err_len = sizeof(int);

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
    buffer *header_out = upstm->header_out;
    buffer *body_out = rqst->body_in;

    request_headers_htop(rqst, header_out);
    /* TODO: use writev instead */
    CONN_WRITE(peer, header_out,
              close_connection(conn));
    if (body_out != NULL) {
        CONN_WRITE(peer, body_out,
                  close_connection(conn));
    }

    LOG_DEBUG("%s upstream write request", conn_str(conn));

    /* set read timeout */
    assert(!peer->write.timer_set);
    timer_add(&peer->read, (timer_msec)upstream_timeout);

    conn_disable_write(peer);
    conn_enable_read(peer, upstream_read_response_header_h);

    upstream_read_response_header_h(&peer->read);
}

static void upstream_read_response_header_h(event *ev)
{
    connection  *peer = ev->conn;
    connection  *conn = peer->peer;

    if (ev->timeout) {
        LOG_WARN("%s upstream response timeout (%dms)",
                 conn_str(conn), upstream_timeout);
        conn_disable_read(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        return;
    }

    /* read upstream http response */
    upstream  *upstm = peer->app;
    buffer    *b = upstm->header_in;
    CONN_READ(peer, b, close_connection(conn));

    upstream_parse_response_h(ev);
}

static void upstream_parse_response_h(event *ev)
{
    connection  *peer = ev->conn;
    connection  *conn = peer->peer;
    upstream    *upstm = peer->app;
    request     *rqst = conn->app;

    int err = upstream_parse(upstm);
    switch (err) {
        case FCY_ERROR:
            LOG_WARN("%s parse upstream response error %s",
                     conn_str(conn), buffer_peek(rqst->header_out));
            conn_disable_read(peer);
            response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);

        case FCY_AGAIN:
            return;

        default:
            break;
    }

    buffer *b = upstm->header_in;
    buffer_retrieve(b, upstm->parser.where);

    if (upstm->content_length > HTTP_MAX_CONTENT_LENGTH) {
        conn_disable_read(peer);
        response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
        LOG_WARN("%s upstream content length too long, %d bytes",
                 conn_str(conn), upstm->content_length);
        return;
    }

    if (!buffer_empty(b)) {
        upstm->avoid_read_body = 1;
        buffer_transfer(upstm->body_in, b);
    }

    peer->read.handler = upstream_read_response_body;
    upstream_read_response_body(ev);
}

static void upstream_read_response_body(event *ev)
{
    connection  *peer = ev->conn;
    connection  *conn = peer->peer;
    upstream    *upstm = peer->app;
    buffer      *b = upstm->body_in;

    if (ev->timeout) {
        LOG_WARN("%s upstream response timeout", conn_str(conn));
        goto error;
    }

    if (upstm->has_content_length_header || upstm->is_chunked) {
        if (upstm->avoid_read_body) {
            upstm->avoid_read_body = 0;
        }
        else {
            CONN_READ(peer, b, close_connection(conn));
        }
    }

    if (upstm->has_content_length_header) {
        if (buffer_readable_bytes(b) < (size_t)upstm->content_length) {
            return;
        }
        else {
            goto done;
        }
    }
    else if (upstm->is_chunked) {
        switch(upstream_read_chunked(upstm)) {
            case FCY_ERROR:
                LOG_ERROR("upstream_read_chunked error");
                goto error;
            case FCY_AGAIN:
                return;
            default:
                goto done;
        }
    }
    else {
        goto done;
    }

    error:
    conn_disable_read(peer);
    response_and_close(conn, STATUS_INTARNAL_SEARVE_ERROR);
    return;

    done:
    if (ev->timer_set) {
        timer_del(ev);
    }

    conn_disable_read(peer);
    conn_enable_write(conn, write_response_all_h);
    write_response_all_h(&conn->write);
}

static void write_response_all_h(event *ev)
{
    connection  *conn = ev->conn;
    request     *rqst = conn->app;
    upstream    *uptm = conn->peer->app;
    buffer      *header_out = rqst->header_out;
    buffer      *body_out = uptm->body_in;

    /* TODO: use writev instead */
    upstream_headers_htop(uptm, header_out);
    CONN_WRITE(conn, header_out,
              close_connection(conn));
    if (body_out != NULL) {
        CONN_WRITE(conn, body_out,
                  close_connection(conn));
    }

    conn_disable_write(conn);
    finalize_request_h(ev);
}

static void write_response_headers_h(event *ev)
{
    connection  *conn = ev->conn;
    request     *rqst = conn->app;
    buffer      *b = rqst->header_out;
    string      *status_str = &status_code_out_str[rqst->status_code];

    /* write header_out */
    if (buffer_empty(b)) {
        /* response line */
        buffer_append_literal(b, "HTTP/1.1 ");
        buffer_append_str(b, status_str);
        buffer_append_literal(b, "\r\nServer: fancy beta");
        buffer_append_literal(b, "\r\nContent-Type: ");

        if (rqst->status_code == STATUS_OK) {
            buffer_append(b, rqst->content_type,
                         strlen(rqst->content_type));
            buffer_append_literal(b, "\r\nContent-Length: ");
            int n = sprintf(buffer_begin_write(b), "%ld", rqst->sbuf.st_size);
            buffer_has_writen(b, (size_t)n);
        } else {
            buffer_append_literal(b, "text/html; charset=utf-8");
            buffer_append_crlf(b);
            buffer_append_str(b, status_str);
        }

        if (rqst->should_keep_alive) {
            buffer_append_literal(b, "\r\nConnection: keep-alive\r\n\r\n");
        } else {
            buffer_append_literal(b, "\r\nConnection: close\r\n\r\n");
        }

        if (rqst->status_code != STATUS_OK) {
            buffer_append_str(b, status_str);
        }
    }

    CONN_WRITE(conn, b,
              close_connection(conn));

    if (rqst->send_fd > 0) {
        ev->handler = send_file_h;
    } else {
        conn_disable_write(conn);
        ev->handler = finalize_request_h;
    }

    ev->handler(ev);
}

static void send_file_h(event *ev)
{
    connection  *conn = ev->conn;
    request     *rqst = conn->app;
    struct stat *sbuf = &rqst->sbuf;

    CONN_SEND_FILE(conn, rqst->send_fd, sbuf,
                  close_connection(conn));

    conn_disable_write(conn);
    finalize_request_h(ev);
}

static void finalize_request_h(event *ev)
{
    connection  *conn = ev->conn;
    request     *rqst = conn->app;
    upstream    *upstm = conn->peer->app;

    if (rqst->is_static || rqst->status_code != STATUS_OK) {
        LOG_DEBUG("%s response \"%s\"",
                  conn_str(conn), status_code_out_str[rqst->status_code].data);
    }
    else {
        LOG_DEBUG("%s response \"%s\"",
                  conn_str(conn), upstm->parser.response_line.data);
    }

    if (!rqst->should_keep_alive || conn->app_count >= keep_alive_requests) {
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
    int listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listenfd == -1) {
        return FCY_ERROR;
    }

    const int on = 1;
    CHECK(setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)));

    CHECK(setsockopt(listenfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &accept_defer, sizeof(accept_defer)));

    struct sockaddr_in  servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    servaddr.sin_port           = htons((uint16_t)listen_on);

    socklen_t addrlen = sizeof(servaddr);
    int err = bind(listenfd, (struct sockaddr*)&servaddr, addrlen);
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