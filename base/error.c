//
// Created by frank on 17-2-12.
//

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include "error.h"

#define MAXLINE 1024

int		daemon_proc;		/* set nonzero by daemon_init() */

static void	err_doit(int, int, const char *, va_list);

static char *timestamp();

void logger(const char *fmt, ...)
{
    va_list     ap;

    printf("%s ",  timestamp());

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
    fflush(stdout);
    return;
}

void logger_client(struct sockaddr_in *addr, const char *fmt, ...)
{
    va_list     ap;

    // TODO:
    printf("%s %s:%hu ",  timestamp(), inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    printf("\n");
    fflush(stdout);
    return;
}

/* Nonfatal error related to system call
 * Print message and return */

void err_ret(const char *fmt, ...)
{
    va_list		ap;

    va_start(ap, fmt);
    err_doit(1, LOG_INFO, fmt, ap);
    va_end(ap);
    return;
}

/* Fatal error related to system call
 * Print message and terminate */

void err_sys(const char *fmt, ...)
{
    va_list		ap;

    va_start(ap, fmt);
    err_doit(1, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(1);
}

/* Fatal error related to system call
 * Print message, dump core, and terminate */

void err_dump(const char *fmt, ...)
{
    va_list		ap;

    va_start(ap, fmt);
    err_doit(1, LOG_ERR, fmt, ap);
    va_end(ap);
    abort();		/* dump core and terminate */
    exit(1);		/* shouldn't get here */
}

/* Nonfatal error unrelated to system call
 * Print message and return */

void err_msg(const char *fmt, ...)
{
    va_list		ap;

    va_start(ap, fmt);
    err_doit(0, LOG_INFO, fmt, ap);
    va_end(ap);
    return;
}

/* Fatal error unrelated to system call
 * Print message and terminate */

void err_quit(const char *fmt, ...)
{
    va_list		ap;

    va_start(ap, fmt);
    err_doit(0, LOG_ERR, fmt, ap);
    va_end(ap);
    exit(1);
}

/* Print message and return to caller
 * Caller specifies "errnoflag" and "level" */

static void err_doit(int errnoflag, int level, const char *fmt, va_list ap)
{
    int		errno_save, n;
    char	buf[MAXLINE + 1];

    errno_save = errno;		/* value caller might want printed */
#ifdef	HAVE_VSNPRINTF
    vsnprintf(header_in, MAXLINE, fmt, ap);	/* safe */
#else
    vsprintf(buf, fmt, ap);					/* not safe */
#endif
    n = strlen(buf);
    if (errnoflag)
        snprintf(buf + n, MAXLINE - n, ": %s", strerror(errno_save));
    strcat(buf, "\n");

    if (daemon_proc) {
        // syslog(level, header_in);
    } else {
        fflush(stdout);		/* header_in case stdout and stderr are the same */
        fputs(buf, stderr);
        fflush(stderr);
    }
    return;
}

static char *timestamp()
{
    struct tm   *tm;
    time_t      rawtime;
    char        *ret;

    time(&rawtime);

    tm = localtime(&rawtime);
    ret = asctime(tm);
    ret[24] = '\0';

    return ret;
}