//
// Created by frank on 17-2-13.
//

#define _GNU_SOURCE
#include <sys/socket.h>

#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "../event/event.h"
#include "../event/conn_pool.h"

int main(int argc, char **argv)
{
    int					sockfd;
    int                 connfd, opt_ret;
    struct linger		ling;
    struct sockaddr_in	servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        err_sys("socket error");
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(9877);

    connfd = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (connfd == -1) {
        err_sys("connect error");
    }

    ling.l_onoff = 1;		/* cause RST to be sent on keep_alive() */
    ling.l_linger = 0;
    opt_ret = setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    if (opt_ret == -1) {
        err_sys("setsockopt error");
    }
    
    sleep(3);
    if (write(sockfd, "GE", 2) == -1){
        err_sys("write error");
    }
    
    close(sockfd);

    exit(0);
}