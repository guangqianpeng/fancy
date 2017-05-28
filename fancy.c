//
// Created by frank on 17-5-27.
//

#include "base.h"
#include "Signal.h"
#include "timer.h"
#include "connection.h"
#include "request.h"
#include "http.h"

static int localfd[2];

static void sig_empty_handler(int signo);
static void sig_quit_handler(int signo);
static int init_worker();
static void run_single_process();

static Message total;

int main()
{
    int     err;

    /* 单进程模式 */
    if (single_process) {
        run_single_process();
    }

    /* 多进程模式 */
    CHECK(socketpair(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0, localfd));

    for (int i = 1; i <= n_workers; ++i) {
        err = fork();
        switch (err) {
            case -1:
                LOG_SYSERR("fork error");
                exit(1);

            case 0:
                msg.worker_id = i;
                CHECK(close(localfd[0]));

                err = init_worker();
                if (err == FCY_ERROR) {
                    LOG_ERROR("init worker error");
                    exit(1);
                }

                LOG_INFO("listening port %d", serv_port);

                Signal(SIGINT, sig_quit_handler);

                event_and_timer_process();

                exit(1);

            default:
                break;
        }
    }

    Signal(SIGINT, sig_empty_handler);

    CHECK(close(localfd[1]));

    for (int i = 1; i <= n_workers; ++i) {
        inter_wait:
        switch (wait(NULL)) {
            case -1:
                if (errno == EINTR) {
                    goto inter_wait;
                }
                else {
                    LOG_ERROR("wait error");
                    exit(1);
                }

            default:
            inter_read:
                if (read(localfd[0], &msg, sizeof(msg)) != sizeof(msg)) {
                    if (errno == EINTR) {
                        goto inter_read;
                    }
                    else if (errno == EAGAIN) {
                        LOG_ERROR("master got bad worker");
                    }
                    else {
                        LOG_ERROR("master read error %s", strerror(errno));
                        exit(1);
                    }
                }

                total.total_connection += msg.total_connection;
                total.total_request += msg.total_request;
                total.ok_request += msg.ok_request;

                LOG_ERROR("worker %d quit:"
                                  "\n\tconnection=%d\n\tok_request=%d\n\tother_request=%d",
                          msg.worker_id, msg.total_connection, msg.ok_request, msg.total_request - msg.ok_request);

                break;
        }
    }

    LOG_INFO("master quit normally:"
                      "\n\tconnection=%d\n\tok_request=%d\n\tother_request=%d",
              total.total_connection, total.ok_request, total.total_request - total.ok_request);
    exit(0);
}

static void run_single_process()
{
    int err = init_worker();
    if (err == FCY_ERROR) {
        LOG_ERROR("init worker error");
        exit(1);
    }

    LOG_INFO("listening port %d", serv_port);

    Signal(SIGINT, sig_quit_handler);

    event_and_timer_process();

    exit(1);
}

static int init_worker()
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

static void sig_empty_handler(int signo)
{
}