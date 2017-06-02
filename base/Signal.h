//
// unp v1
//

#ifndef FANCY_SIGNAL_H
#define FANCY_SIGNAL_H

#include "base.h"

typedef	void	Sigfunc(int);
int Signal(int signo, Sigfunc *func);

#endif //FANCY_SIGNAL_H
