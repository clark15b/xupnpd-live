/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "defaults.h"
#include "ev_base.h"
#include <stdlib.h>

ev::socket::~socket(void)
{
    if(fd!=-1)
        close(fd);
}

ev::source::~source(void)
{
    if(buf)
        free(buf);
}

bool ev::source::set_buf_size(int len)
{
    if(len<1)
        return false;

    if(buf)
        free(buf);

    len=defaults::ts_align_right(len);

    buf=(char*)malloc(len);

    if(!buf)
        return false;

    size=len;

    return true;
}

ev::client* ev::scheduler::insert(client* c,time_t expiry)
{
    c->tm_prev=c->tm_next=NULL;

    c->tm_expiry=expiry;

    if(!beg)
        beg=end=c;
    else
        { end->tm_next=c; c->tm_prev=end; end=c; }

    return c;
}

ev::client* ev::scheduler::remove(client* c)
{
    if(!c->tm_expiry)
        return c;

    if(c->tm_next)
        c->tm_next->tm_prev=c->tm_prev;
    else
        end=c->tm_prev;

    if(c->tm_prev)
        c->tm_prev->tm_next=c->tm_next;
    else
        beg=c->tm_next;

    c->tm_prev=c->tm_next=NULL;

    c->tm_expiry=0;

    return c;
}

ev::client* ev::scheduler::fetch(time_t now)
{
    if(!beg || beg->tm_expiry>now)
        return NULL;

    client* c=beg;

    beg=beg->tm_next;

    if(beg)
        beg->tm_prev=NULL;

    c->tm_prev=c->tm_next=NULL;

    c->tm_expiry=0;

    return c;
}

time_t ev::scheduler::get_wake_delay(time_t now)
{
    if(!beg)
        return 0;

    return beg->tm_expiry-now;
}
