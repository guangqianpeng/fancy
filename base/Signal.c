//
// unp v1
//

#include "Signal.h"

Sigfunc *my_signal(int signo, Sigfunc *func)
{
    struct sigaction	act, oact;

    act.sa_handler = func;
    sigemptyset(&act.sa_mask);	/* 不阻塞其他信号，POSIX保证被捕获信号总是被阻塞 */
    act.sa_flags = 0;
    if (signo == SIGALRM) {
#ifdef	SA_INTERRUPT
        act.sa_flags |= SA_INTERRUPT;	/* SunOS 4.x */
#endif
    } else {
#ifdef	SA_RESTART
        act.sa_flags |= SA_RESTART;		/* SVR4, 44BSD */
#endif
    }
    if (sigaction(signo, &act, &oact) < 0)
        return(SIG_ERR);
    return(oact.sa_handler);
}
/* end signal */

Sigfunc *Signal(int signo, Sigfunc *func)	/* for our signal() function */
{
    Sigfunc	*sigfunc;

    if ( (sigfunc = my_signal(signo, func)) == SIG_ERR) {
        perror("signal error");
    }
    return (sigfunc);
}