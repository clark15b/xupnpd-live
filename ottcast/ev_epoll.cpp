/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "ev_epoll.h"
#include <sys/epoll.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

volatile int ev_epoll::engine::__sig_quit=0;

volatile int ev_epoll::engine::__sig_alrm=0;

volatile int ev_epoll::engine::__sig_chld=0;

void ev_epoll::engine::__sig_handler(int n)
{
   switch(n)
    {
    case SIGINT:
    case SIGQUIT:
    case SIGTERM: __sig_quit=1; break;
    case SIGALRM: __sig_alrm=1; break;
    case SIGCHLD: __sig_chld=1; break;
    }
}

void ev_epoll::engine::initialize(void)
{
    struct signal_handler { int signum; sighandler_t handler; };

    static const signal_handler signal_tbl[]=
    {
        { SIGHUP,  SIG_IGN       },
        { SIGINT,  __sig_handler },
        { SIGQUIT, __sig_handler },
        { SIGPIPE, SIG_IGN       },
        { SIGALRM, __sig_handler },
        { SIGTERM, __sig_handler },
        { SIGUSR1, SIG_IGN       },
        { SIGUSR2, SIG_IGN       },
        { SIGCHLD, __sig_handler },
        { 0      , NULL          }
    };

    struct sigaction act;

    sigfillset(&act.sa_mask);

    act.sa_flags=0;

    for(int i=0;signal_tbl[i].handler;i++)
    {
        act.sa_handler=signal_tbl[i].handler;

        sigaction(signal_tbl[i].signum,&act,NULL);
    }
}

ev_epoll::engine::engine(void)
{
    efd=epoll_create(100);
}

ev_epoll::engine::~engine(void)
{
    if(efd!=-1)
        close(efd);
}

bool ev_epoll::engine::set(ev::socket* s)
{
    if(efd==-1)
        return false;

    epoll_event event;

    event.data.ptr=(void*)s;

    event.events=EPOLLRDHUP|EPOLLPRI;

    if(!s->is_pollin)
        event.events|=EPOLLIN;

    if(!s->is_pollout)
        event.events|=EPOLLOUT;

    int rc;

    if(s->is_monitored)
        rc=epoll_ctl(efd,EPOLL_CTL_MOD,s->fd,&event);
    else
        { s->is_monitored=true; rc=epoll_ctl(efd,EPOLL_CTL_ADD,s->fd,&event); }

    return rc?false:true;
}

bool ev_epoll::engine::del(ev::socket* s)
{
    if(efd==-1 || !s->is_monitored)
        return false;

    s->is_monitored=false;

    epoll_event event;

    return epoll_ctl(efd,EPOLL_CTL_DEL,s->fd,&event)?false:true;
}

void ev_epoll::engine::loop(ev::controller* ctrl)
{
    setsid();

    sigset_t fullset,emptyset;

    sigfillset(&fullset); sigemptyset(&emptyset);

    sigprocmask(SIG_SETMASK,&fullset,NULL);

    static epoll_event events[100];

    while(!__sig_quit)
    {
        int rc=epoll_pwait(efd,events,sizeof(events)/sizeof(*events),-1,&emptyset);

        if(rc==-1)
        {
            if(errno==EINTR)
            {
                if(__sig_chld)
                {
                    pid_t pid; int status;

                    while((pid=wait4(-1,&status,WNOHANG,NULL))>0)
                        ctrl->on_chld(pid);

                    __sig_chld=0;
                }

                if(__sig_alrm)
                    { ctrl->on_alrm(); __sig_alrm=0; }

                ctrl->flush();

                continue;
            }else
                break;
        }

        for(int i=0;i<rc;i++)
        {
            ev::socket* s=(ev::socket*)events[i].data.ptr;

            u_int32_t evs=events[i].events;

            if(evs & EPOLLIN)
                s->is_pollin=true;

            if(evs & EPOLLOUT)
                s->is_pollout=true;

            if(evs & (EPOLLPRI|EPOLLRDHUP|EPOLLERR|EPOLLHUP))
                s->is_pollhup=true;

            ctrl->on_event(s);
        }

        ctrl->flush();
    }

    sigprocmask(SIG_SETMASK,&emptyset,NULL);

    signal(SIGTERM,SIG_IGN);

    signal(SIGCHLD,SIG_IGN);

    kill(0,SIGTERM);
}
