//
// Created by frank on 17-6-1.
//

#include "log.h"
#include "timer.h"
#include "Signal.h"
#include "connection.h"
#include "http.h"
#include "request.h"
#include "config.h"
#include "cycle.h"
#include <sys/signalfd.h>

#define SIG_FCY_QUIT    SIGUSR1
#define SIG_FCY_RELOAD  SIGHUP

static mem_pool *pool;

static volatile sig_atomic_t sig_quit;
static volatile sig_atomic_t sig_reload;
static volatile sig_atomic_t sig_other;

static int worker_init();
static void worker_signal_init();
static void signal_handler(int sig_no);

void run_master_process()
{
    sigset_t    mask;

    CHECK(sigemptyset(&mask));
    CHECK(sigaddset(&mask, SIGCHLD));
    CHECK(sigprocmask(SIG_BLOCK, &mask, NULL));

    LOG_INFO("master start");
    for (int i = 0; i < worker_processes; ++i) {
        switch (fork()) {
            case -1:
                perror("run master process fork error");
                exit(EXIT_FAILURE);

            case 0:
                CHECK(close(pid_fd));
                run_single_process();
                exit(EXIT_SUCCESS);

            default:
                break;
        }
    }

    CHECK(sigaddset(&mask, SIGINT));
    CHECK(sigaddset(&mask, SIGQUIT));
    CHECK(sigaddset(&mask, SIG_FCY_RELOAD));
    CHECK(sigprocmask(SIG_BLOCK, &mask, NULL));

    /* do not kill yourself */
    CHECK(Signal(SIG_FCY_QUIT, SIG_IGN));

    int sfd = signalfd(-1, &mask, 0);
    if (sfd == -1) {
        perror("sigalfd error");
        exit(EXIT_FAILURE);
    }

    int rest_processes = worker_processes;
    while (rest_processes > 0) {

        int pid, wstatus;
        struct signalfd_siginfo fdsi;

        ssize_t s = read(sfd, &fdsi, sizeof(struct signalfd_siginfo));
        if (s != sizeof(struct signalfd_siginfo)) {
            LOG_SYSERR("read signalfd_siginfo error");
            continue;
        }

        LOG_DEBUG("recv signal [%s]", strsignal(fdsi.ssi_signo));

        switch (fdsi.ssi_signo) {
            case SIG_FCY_RELOAD:
                LOG_ERROR("reload not implemented");
                sig_reload = 0;
                break;
            case SIGINT:
            case SIGQUIT:
                CHECK(kill(0, SIG_FCY_QUIT));
                LOG_DEBUG("send signal [%s]", strsignal(SIG_FCY_QUIT));
                sig_quit = 0;
                break;
            case SIGCHLD:
                while ((pid = waitpid(0, &wstatus, WNOHANG)) > 0) {
                    if (WEXITSTATUS(wstatus) == EXIT_FAILURE) {
                        LOG_ERROR("worker [%d] exit failure", pid);
                    } else {
                        LOG_DEBUG("worker [%d] exit success", pid);
                    }
                    --rest_processes;
                }
                if (pid == -1 && errno != ECHILD) {
                    LOG_SYSERR("waitpid error");
                }
                break;
            default:
                LOG_ERROR("recv unknown signal %s",
                          strsignal(fdsi.ssi_signo));
                break;
        }
    }
}

void run_single_process()
{
    if (worker_init() == FCY_ERROR) {
        fprintf(stderr, "init worker error");
        exit(EXIT_FAILURE);
    }

    LOG_INFO("worker listening port %d", listen_on);

    while (1) {
        event_and_timer_process();
        if (sig_quit) {
            break;
        }
        if (sig_reload) {
            LOG_ERROR("reload not implemented");
            sig_reload = 0;
        }
        if (sig_other) {
            LOG_ERROR("unexpected signal: %s", strsignal(sig_other));
            sig_other = 0;
        }
    }
    mem_pool_destroy(pool);
}

void run_signal_process(int sig_no)
{
    int fd = open(FANCY_PID_FILE, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "open %s error: %s",
                FANCY_PID_FILE, strerror(errno));
    }

    char pid_str[32] = {};
    if (read(fd, pid_str, 32) == -1) {
        perror("read pid file error");
    }

    if (!isdigit(pid_str[0])) {
        fprintf(stderr, "bad pid file, not integer");
    }

    int pid = atoi(pid_str);
    if (kill(pid, sig_no) == -1) {
        perror("signal send error");
    }
}

static int worker_init()
{
    size_t size = worker_connections * sizeof (connection) + sizeof(mem_pool);
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
        mem_pool_destroy(pool);
        return FCY_ERROR;
    }

    worker_signal_init();

    return FCY_OK;
}

static void worker_signal_init()
{
    CHECK(Signal(SIGPIPE, SIG_IGN));
    CHECK(Signal(SIG_FCY_QUIT, signal_handler));
    CHECK(Signal(SIG_FCY_RELOAD, signal_handler));
}

static void signal_handler(int sig_no)
{
    switch (sig_no) {
        case SIG_FCY_QUIT:
            sig_quit = 1;
            break;
        case SIG_FCY_RELOAD:
            sig_reload = 1;
            break;
        default:
            sig_other = sig_no;
            break;
    }
}