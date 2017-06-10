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
#include "cycle.h"

static int open_and_test_file(const char *path);
static int write_and_lock_file(const char *path);

int main(int argc, char **argv)
{
    int signal_process = 0;
    int sig_quit = 0;
    int sig_reload = 0;

    int opt;
    while ((opt = getopt(argc, argv, "s:")) != -1) {
        switch (opt) {
            case 's':
                signal_process = 1;
                if (strcmp(optarg, "reload") == 0) {
                    sig_reload = 1;
                }
                else if (strcmp(optarg, "quit") == 0) {
                    sig_quit = 1;
                }
                else {
                    fprintf(stderr, "Usage: %s [-s quit]\n", argv[0]);
                    exit(EXIT_FAILURE);
                }
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-s quit]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (signal_process) {
        if (sig_reload) {
            run_signal_process(SIGHUP);
        }
        else if (sig_quit) {
            run_signal_process(SIGQUIT);
        }
        exit(EXIT_SUCCESS);
    }

    if (open_and_test_file(FANCY_PID_FILE) == FCY_ERROR) {
        fprintf(stderr, "fancy already running");
        exit(EXIT_FAILURE);
    }

    config(FANCY_CONFIG_FILE);

    if (log_init(&log_path) == FCY_ERROR) {
        fprintf(stderr, "init log error");
        exit(EXIT_FAILURE);
    }

    if (daemonize) {
        if (daemon(1, 0) == -1) {
            LOG_SYSERR("daemonize error");
            exit(EXIT_FAILURE);
        }
    }

    if (write_and_lock_file(FANCY_PID_FILE) == FCY_ERROR) {
        LOG_ERROR("create pid file error");
        exit(EXIT_FAILURE);
    }

    /* 单进程模式 */
    if (!master_process) {
        run_single_process();
        LOG_INFO("quit");
    }
    else {
        run_master_process();
        LOG_INFO("master exit success");
    }
}

static int open_and_test_file(const char *path)
{
    int pid_fd = open(path, O_RDWR | O_CREAT ,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (pid_fd == -1) {
        perror("open pid file error");
        exit(EXIT_FAILURE);
    }

    if (lockf(pid_fd, F_TLOCK, 0) == -1) {
        if (errno == EAGAIN || errno == EACCES) {
            CHECK(close(pid_fd));
            return FCY_ERROR;
        }
        fprintf(stderr, "can not lock file %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    CHECK(lockf(pid_fd, F_ULOCK, 0));
    CHECK(close(pid_fd));

    return FCY_OK;
}

static int write_and_lock_file(const char *path)
{
    int pid_fd = open(path, O_RDWR | O_CREAT ,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (lockf(pid_fd, F_TLOCK, 0) == -1) {
        if (errno == EAGAIN || errno == EACCES) {
            CHECK(close(pid_fd));
            return FCY_ERROR;
        }
        fprintf(stderr, "can not lock file %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    CHECK(ftruncate(pid_fd, 0));

    if (dprintf(pid_fd, "%d", getpid()) == -1) {
        LOG_ERROR("dprintf error");
        CHECK(close(pid_fd));
        return FCY_ERROR;
    }

    return FCY_OK;
}
