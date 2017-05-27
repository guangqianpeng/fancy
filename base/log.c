//
// Created by frank on 17-2-12.
//

#include <stdio.h>
#include <stdarg.h>
#include "log.h"

#define MAXLINE     1024

const static char *log_level_str[] = {
        "[DEBUG]",
        "[INFO] ",
        "[WARN] ",
        "[ERROR]",
        "[FATAL]"
};

static int timestamp(char *);

void log_init(const char *file_name)
{
    int fd = open(file_name, O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        fprintf(stderr, "open log file %s error", file_name);
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
    i += vsnprintf(data + i,MAXLINE - i - 32, fmt, ap);
    va_end(ap);

    snprintf(data + i, MAXLINE - i, " - %s:%d", strrchr(file, '/') + 1, line);

    dprintf(log_fd, "%s\n", data);

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
    i += sprintf(data + i, " %s ", "SYSERROR");

    va_start(ap, fmt);
    i += vsnprintf(data + i,MAXLINE - i - 32, fmt, ap);
    va_end(ap);

    i += snprintf(data + i, MAXLINE - i, ": %s", strerror(errno));
    snprintf(data + i, MAXLINE - i, " - %s:%d", strrchr(file, '/') + 1, line);

    dprintf(log_fd, "%s\n", data);

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