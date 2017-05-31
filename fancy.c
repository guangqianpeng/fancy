//
// Created by frank on 17-5-27.
//

#include "base.h"
#include "log.h"
#include "Signal.h"
#include "timer.h"
#include "connection.h"
#include "request.h"
#include "http.h"
#include "config.h"

static int          localfd[2];
static mem_pool     *pool;
static int init_pool();
static int worker_init();
static void run_single_process();

int main()
{
    int     err;

    if (init_pool() == FCY_ERROR) {
        fprintf(stderr, "init pool error");
        exit(1);
    }

    config("fancy.conf", pool);

    if (log_init(log_path) == FCY_ERROR) {
        fprintf(stderr, "init log error");
        exit(1);
    }

    /* 单进程模式 */
    if (!master_process) {
        run_single_process();
    }

    /* 多进程模式 */
    CHECK(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, localfd));

    for (int i = 1; i <= worker_processes; ++i) {
        err = fork();
        switch (err) {
            case -1:
                LOG_SYSERR("fork error");
                exit(1);

            case 0:
                CHECK(close(localfd[0]));

                err = worker_init();
                if (err == FCY_ERROR) {
                    LOG_ERROR("init worker error");
                    exit(1);
                }

                LOG_INFO("listening port %d", listen_on);

                event_and_timer_process();

                exit(1);

            default:
                break;
        }
    }

    CHECK(close(localfd[1]));

    int ret;
    for (int i = 1; i <= worker_processes; ++i) {
        inter_wait:

        switch (ret = wait(NULL)) {
            case -1:
                if (errno == EINTR) {
                    goto inter_wait;
                }
                else {
                    LOG_ERROR("wait error");
                    exit(1);
                }

            default:
                LOG_INFO("worker %d quit", ret);
                break;
        }
    }

    LOG_INFO("master quit");
}

static void run_single_process()
{
    if (worker_init() == FCY_ERROR) {
        fprintf(stderr, "init worker error");
        exit(1);
    }

    LOG_INFO("listening port %d", listen_on);

    event_and_timer_process();

    exit(1);
}

static int init_pool()
{
    size_t size = 102400 * sizeof (connection) + sizeof(mem_pool);
    pool = mem_pool_create(size);
    if (pool == NULL) {
        return FCY_ERROR;
    }
    return FCY_OK;
}

static int worker_init()
{
    size_t      size;

    size = worker_connections * sizeof (connection) + sizeof(mem_pool);
    pool = mem_pool_create(size);

    if (pool == NULL){
        return FCY_ERROR;
    }

    if (conn_pool_init(pool, worker_connections) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (event_init(pool, epoll_events) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    timer_init();

    if (request_init(pool) == FCY_ERROR) {
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    if (accept_init() == FCY_ERROR) {
        return FCY_ERROR;
    }

    Signal(SIGPIPE, SIG_IGN);

    return FCY_OK;
}