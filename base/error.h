//
// Created by frank on 17-2-12.
//

#ifndef FANCY_ERROR_H
#define FANCY_ERROR_H

#include "base.h"

void	 err_dump(const char *, ...);
void	 err_msg(const char *, ...);
void	 err_quit(const char *, ...);
void	 err_ret(const char *, ...);
void	 err_sys(const char *, ...);

#endif //FANCY_ERROR_H
