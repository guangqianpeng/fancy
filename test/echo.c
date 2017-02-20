//
// Created by frank on 17-2-12.
//

#include "base.h"
#include "app.h"
#include "timer.h"
#include "conn_pool.h"

#define CONN_MAX            128
#define EVENT_MAX           128
#define REQUEST_TIMEOUT     50000
#define SERV_PORT           9877


static void sig_handler(int signo);
static void empty_handler(event *ev);
static void read_handler(event *ev);
static void write_handler(event *ev);
static void accept_handler(event *ev);

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

    /* 事件循环 */
    while (1) {

        timeout = timer_recent();

        n_ev = event_process(timeout);

        if (n_ev == FCY_ERROR) {
            break;
        }

        timer_process();
    }

    err_msg("echo quit normally");
}

static void sig_handler(int signo)
{
}

static void empty_handler(event *ev)
{
}

static void read_handler(event *ev)
{
    ssize_t     n;
    connection  *conn;
    int         fd;
    buffer      *buf;

    conn = ev->conn;
    fd = conn->fd;
    buf = conn->buf;

    if (ev->timeout) {
        err_msg("read_handler keep_alive connection %d, timeout\n", fd);
        close_connection(conn);
        return;
    }

    /* read之前保证所有的buffer都发送出去了 */
    assert(buffer_empty(buf));

    while ( (n = read(fd, buf->data_end, buf->end - buf->data_end)) == -1) {
        if (errno == EINTR) {
            continue;
        }
        break;
    }

    if (n <= 0) {
        if (n == -1 && errno == EAGAIN) { // 正常返回
            if (!ev->timer_set) {
                timer_add(ev, REQUEST_TIMEOUT);
            }
            return;
        }
        else if (n == 0 || errno == ECONNRESET) { // 对端reset或fin
            // 对于fin不必考虑半关闭情况，因为缓冲区保证为空
            err_msg("read_handler keep_alive connection %d, recv %s\n", fd, n == 0 ? "fin":"reset");
            close_connection(conn);
            return;
        }
        else {  // 未知错误
            err_sys("read error");
        }
    }

    /* n > 0 */

    /* BUG: 在用chargen服务测试时，run echo不久会出现segment fault
     * 若删除err_msg则不会出现
     * */
    err_msg("read from fd %d: %ld bytes", fd, n);
    buffer_seek_end(buf, (int)n);

    /* 消除计时器 */
    if (ev->timer_set) {
        timer_del(ev);
    }

    ev->handler = empty_handler;

    /* 此处调用write_handler原因:
     * epoll是边沿触发，最少情况下，write_handler被epoll_wait调用一次
     *
     * 另外，write_handler和read_handler是尾递归调用
     * gcc -O2帮我们消除了尾递归(见echo.s)
     * */
    conn->write->handler = write_handler;
    write_handler(conn->write);
}

static void write_handler(event *ev)
{
    ssize_t     n = 0;
    connection  *conn;
    int         fd;
    buffer      *buf;
    char        *data;

    conn = ev->conn;
    fd = conn->fd;
    buf = conn->buf;

    data = buffer_read(buf);

    while (!buffer_empty(buf)) {
        n = write(fd, data, buffer_size(buf));
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        data = buffer_seek_start(buf, n);
    }

    if (n == -1) {
        if (errno == EWOULDBLOCK) { /* 正常结束, 等epoll_wait */
            return;
        }
        else if (errno == EPIPE || errno == ECONNRESET) {   /* 对端reset连接 */
            err_msg("write_headers_handler close connection %d, %s\n", fd, errno == EPIPE ? "pip":"reset");
            close_connection(conn);
            return;
        }
        else {  /* 未知错误 */
            err_sys("write error");
        }
    }

    /* n >= 0 */
    assert(buffer_empty(buf));
    buffer_reset(buf);


    /* 此处调用read_handler原因:
     * buffer中可能仍有数据，然而epoll是边沿触发，不会再激活读事件
     * */
    conn->read->handler = read_handler;
    read_handler(conn->read);
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

    len = sizeof(addr);

    inter:
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
}

static void close_connection(connection *conn)
{
    int fd = conn->fd;

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
}