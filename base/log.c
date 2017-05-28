//
// Created by frank on 17-2-12.
//

#include <stdio.h>
#include <stdarg.h>
#include "log.h"

#define MAXLINE     256

const static char *log_level_str[] = {
        "[DEBUG]",
        "[INFO] ",
        "[WARN] ",
        "[ERROR]",
        "[FATAL]"
};

static int log_fd = STDOUT_FILENO;

static int timestamp(char *);

void log_init(const char *file_name)
{
    log_fd = open(file_name, O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (log_fd == -1) {
        fprintf(stderr, "open log file %s error: %s",
                file_name, strerror(errno));
    }
}

void log_base(const char *file,
              int line,
              int level,
              int to_abort,
              const char *fmt, ...)
{
    char        data[MAXLINE];
    size_t      i = 0;
    va_list     ap;

    i += timestamp(data);
    i += sprintf(data + i, " %d", getpid());
    i += sprintf(data + i, " %s ", log_level_str[level]);

    va_start(ap, fmt);
    vsnprintf(data + i, MAXLINE - i, fmt, ap);
    va_end(ap);

    dprintf(log_fd, "%s - %s:%d\n", data, strrchr(file, '/') + 1, line);

    if (to_abort) {
        abort();
    }
}

void log_sys(const char *file,
             int line,
             int to_abort,
             const char *fmt, ...)
{
    char        data[MAXLINE];
    size_t      i = 0;
    va_list     ap;

    i += timestamp(data);
    i += sprintf(data + i, " %d", getpid());
    i += sprintf(data + i, " %s ", to_abort ? "[SYSFA]":"[SYSER]");

    va_start(ap, fmt);
    vsnprintf(data + i, MAXLINE - i, fmt, ap);
    va_end(ap);

    dprintf(log_fd, "%s: %s - %s:%d\n", data, strerror(errno), strrchr(file, '/') + 1, line);

    if (to_abort) {
        abort();
    }
}

static int timestamp(char *data)
{
    struct tm   *tm;
    time_t      rawtime;

    time(&rawtime);
    tm = localtime(&rawtime);
    asctime_r(tm, data);

    return 24;
}