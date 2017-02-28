//
// Created by frank on 17-2-19.
//

#include <sys/epoll.h>
#include "app.h"
#include "conn_pool.h"
#include "timer.h"

int n_connections       = 256;
int n_events            = 128;
int request_timeout     = 10000;
int serv_port           = 9877;
int use_accept_mutex    = 0;
int accept_dealy        = 10;
int single_process      = 0;
int n_workers           = 4;

static pthread_mutex_t  *accept_mutex;
static int              listenfd;

extern int              n_free_connections;
static int              accept_mutex_held = 0;
static int              disable_accept = 0;
static event            *accept_event;
static int              listenfd;

static int init_accept_mutex();
static int trylock_accept_mutex();
static int unlock_accept_mutex();

static int tcp_listen(int serv_port);
static int init_and_add_accept_event(event_handler accept_handler);

/* master运行 */
int init_server()
{
    int         err;

    /* 初始化所有参数 */
    n_free_connections = n_connections;
    // ...初始化其他参数
    // ...


    if (use_accept_mutex) {
        err = init_accept_mutex();
        if (err == FCY_ERROR) {
            logger("init_accept_mutex error");
            return FCY_ERROR;
        }
    }

    listenfd = tcp_listen(serv_port);

    return (listenfd == FCY_ERROR ? FCY_ERROR : FCY_OK);
}

int init_worker(event_handler accept_handler)
{
    mem_pool    *pool;
    size_t      size;

    size = n_connections * sizeof (connection) + sizeof(mem_pool);
    pool = mem_pool_create(size);

    if (pool == NULL){
        return FCY_ERROR;
    }

    if (conn_pool_init(pool, n_connections) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (event_init(pool, n_events) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    timer_init();

    if (init_and_add_accept_event(accept_handler) == FCY_ERROR) {
        logger("add_aceept_event error");
        return FCY_ERROR;
    }

    signal(SIGPIPE, SIG_IGN);

    return FCY_OK;
}

void event_and_timer_process()
{
    timer_msec  timeout = (timer_msec)-1;
    int         n_ev;

    while (1) {

        if (use_accept_mutex) {
            if (disable_accept > 0) {
                --disable_accept;
            }
            else {
                // 抢锁并监听accept事件
                if (trylock_accept_mutex() == FCY_ERROR) {
                    logger("trylock_accept_mutex error");
                    return;
                }
                // 没抢到锁
                if (!accept_mutex_held) {
                    timeout = (timer_msec)accept_dealy;
                }
                else {
                    disable_accept = n_connections / 8 - n_free_connections;
                }
            }
        }

        n_ev = event_process(timeout);

        if (n_ev == FCY_ERROR) {
            logger("event_process error");
            return;
        }

        if (accept_mutex_held) {
            if (unlock_accept_mutex() == FCY_ERROR) {
                logger("unlock_accept_mutex error");
                return;
            }
        }

        timer_process();
        timeout = timer_recent();
    }
}

static int init_accept_mutex()
{
    pthread_mutexattr_t     attr;

    accept_mutex = mmap(NULL, sizeof(*accept_mutex), PROT_READ | PROT_WRITE, MAP_ANONYMOUS |MAP_SHARED, -1, 0);
    if (accept_mutex == MAP_FAILED) {
        logger("mmap error %s", strerror(errno));
        return FCY_ERROR;
    }

    /* 没有检查返回值， 调用确定返回0 */
    (void) pthread_mutexattr_init(&attr);

    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
        (void) pthread_mutexattr_destroy(&attr);
        return FCY_ERROR;
    }

    (void) pthread_mutex_init(accept_mutex, &attr);
    (void) pthread_mutexattr_destroy(&attr);

    return FCY_OK;
}

static int trylock_accept_mutex()
{
    int err;

    assert(!accept_mutex_held);

    err = pthread_mutex_trylock(accept_mutex);
    if (err == 0) {

        /* accept采用水平触发 */
        accept_event->conn->fd = listenfd;
        if (event_add(accept_event, EPOLLERR) == FCY_ERROR) {
            return FCY_ERROR;
        }

        accept_mutex_held = 1;
        return FCY_OK;
    }
    else if (err == EBUSY) {
        return FCY_OK;
    }
    else {
        return FCY_ERROR;
    }
}

static int unlock_accept_mutex()
{
    int err;

    assert(accept_mutex_held);

    err = pthread_mutex_unlock(accept_mutex);
    if (err != 0) {
        return FCY_ERROR;
    }

    err = event_del(accept_event, 0);
    if (err == FCY_ERROR) {
        return FCY_ERROR;
    }

    accept_mutex_held = 0;
    return FCY_OK;
}


static int tcp_listen(int serv_port)
{
    int                 listenfd;
    struct sockaddr_in  servaddr;
    socklen_t           addrlen;
    const int           sockopt = 1;
    int                 err;

    listenfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listenfd == -1) {
        logger("socket error %s", strerror(errno));
        return FCY_ERROR;
    }

    err = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
    if (err == -1) {
        logger("setsockopt error %s", strerror(errno));
        return FCY_ERROR;
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family         = AF_INET;
    servaddr.sin_addr.s_addr    = htonl(INADDR_ANY);
    servaddr.sin_port           = htons((uint16_t)serv_port);

    addrlen = sizeof(servaddr);
    err = bind(listenfd, (struct sockaddr*)&servaddr, addrlen);
    if (err == -1) {
        logger("bind error %s", strerror(errno));
        return FCY_ERROR;
    }

    err = listen(listenfd, 1024);
    if (err == -1) {
        logger("listen error %s", strerror(errno));
        return FCY_ERROR;
    }

    return listenfd;
}

int init_and_add_accept_event(event_handler accept_handler)
{
    connection  *conn;

    conn = conn_pool_get();
    if (conn == NULL) {
        return FCY_ERROR;
    }

    conn->fd = listenfd;
    conn->read->handler = accept_handler;

    /* 若不使用accept mutex，则每个worker监听端口 */
    if (!use_accept_mutex) {
        if (event_add(conn->read, EPOLLERR) == FCY_ERROR) {
            conn_pool_free(conn);
            return FCY_ERROR;
        }
    }

    accept_event = conn->read;
    return FCY_OK;
}