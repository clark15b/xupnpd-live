/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "common.h"
#include <stddef.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <setjmp.h>
#include <string.h>
#include <unistd.h>

namespace common
{
    jmp_buf __jmp_lookup_env;

    void __jmp_lookup_handler(int)
        { siglongjmp(__jmp_lookup_env,1); }

    void __connect_handler(int) {}
}

void common::signal(int signum,sighandler_t handler)
{
    struct sigaction act;

    sigfillset(&act.sa_mask);

    act.sa_flags=0;

    act.sa_handler=handler;

    sigaction(signum,&act,NULL);
}

void common::reset_signals(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT,SIG_DFL);
    signal(SIGALRM,SIG_DFL);
    signal(SIGTERM,SIG_DFL);
    signal(SIGCHLD,SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGUSR1,SIG_DFL);
    signal(SIGUSR2,SIG_DFL);
    signal(SIGPIPE,SIG_IGN);

    sigset_t emptyset; sigemptyset(&emptyset);

    sigprocmask(SIG_SETMASK,&emptyset,NULL);
}

in_addr_t common::lookup(const char* s,int timeout)
{
    in_addr_t addr=inet_addr(s);

    if(addr!=INADDR_NONE)
        return addr;

    if(!sigsetjmp(__jmp_lookup_env,1))
    {
        struct sigaction act,oldact;

        act.sa_handler=__jmp_lookup_handler; sigfillset(&act.sa_mask); act.sa_flags=SA_RESETHAND;

        sigaction(SIGALRM,&act,&oldact); alarm(timeout);

        hostent* he=gethostbyname(s);

        alarm(0); sigaction(SIGALRM,&oldact,NULL);

        if(he)
            memcpy((char*)&addr,he->h_addr,sizeof(in_addr_t));
    }else
        return INADDR_NONE;

    return addr;
}

int common::connect(const sockaddr_in* sin,int timeout)
{
    int fd=socket(PF_INET,SOCK_STREAM,0);

    if(fd==-1)
        return -1;

    struct sigaction act,oldact;

    act.sa_handler=__connect_handler; sigfillset(&act.sa_mask); act.sa_flags=SA_RESETHAND;

    sigaction(SIGALRM,&act,&oldact); alarm(timeout);

    int rc=::connect(fd,(sockaddr*)sin,sizeof(sockaddr_in));

    alarm(0); sigaction(SIGALRM,&oldact,NULL);

    if(!rc)
        return fd;

    ::close(fd);

    return -1;
}
