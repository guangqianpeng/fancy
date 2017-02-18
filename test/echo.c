//
// Created by frank on 17-2-12.
//

#include "base.h"
#include "timer.h"
#include "conn_pool.h"

#define CONN_MAX            128
#define EVENT_MAX           128
#define REQUEST_TIMEOUT     50000

static int init_echo();
static int add_accept_event();

static void sig_handler(int signo);
static void empty_handler(event *ev);
static void read_handler(event *ev);
static void write_handler(event *ev);
static void accept_handler(event *ev);

static int tcp_listen();
static void close_connection(connection *conn);

int main()
{
    timer_msec  timeout;
    int         n_ev;


    if (init_echo() == FCY_ERROR) {
        err_quit("init_echo error");
    }

    if (add_accept_event() == FCY_ERROR) {
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

static int init_echo()
{
    mem_pool    *pool;

    pool = mem_pool_create(MEM_POOL_DEFAULT_SIZE);
    if (pool == NULL){
        return FCY_ERROR;
    }

    if (conn_pool_init(pool, CONN_MAX) == -1) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (event_init(pool, EVENT_MAX) == -1) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    timer_init();

    return FCY_OK;
}

static int add_accept_event()
{
    int         listenfd;
    connection  *conn;

    listenfd = tcp_listen();

    conn = conn_pool_get();
    if (conn == NULL) {
        return FCY_ERROR;
    }

    conn->fd = listenfd;
    conn->read->handler = accept_handler;

    if (event_add(conn->read) == FCY_ERROR) {
        return FCY_ERROR;
    }

    return FCY_OK;
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
    buffer_seek_end(buf, n);

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
            err_msg("write_headers_handler keep_alive connection %d, %s\n", fd, errno == EPIPE ? "pip":"reset");
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

static int tcp_listen()
{
    int                 listenfd;
    struct sockaddr_in  servaddr;
    socklen_t           addrlen;
    const int           sockopt;
    int                 err;

    listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenfd == -1) {
        err_sys("socket error");
    }

    err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    if (err == -1) {
        err_sys("setsockopt error");
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    servaddr.sin_port           = htons(9877);

    addrlen = sizeof(servaddr);
    err = bind(listenfd, (struct sockaddr*)&servaddr, addrlen);
    if (err == -1) {
        err_sys("bind error");
    }

    err = listen(listenfd, 1024);
    if (err == -1) {
        err_sys("listen error");
    }

    return listenfd;
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