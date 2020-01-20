/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __COMMON_H
#define __COMMON_H

#include <signal.h>
#include <netinet/in.h>

namespace common
{
#if defined(__FreeBSD__) || defined(__APPLE__)
    typedef void (*sighandler_t)(int);
#endif

    void signal(int signum,sighandler_t handler);

    void reset_signals(void);

    in_addr_t lookup(const char* s,int timeout);

    int connect(const sockaddr_in* sin,int timeout);
}

#endif /* __COMMON_H */
