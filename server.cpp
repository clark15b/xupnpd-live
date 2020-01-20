/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "server.h"
#include "worker.h"
#include "producer.h"
#include "config.h"
#include "common.h"
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

volatile int ott::server::__sig_quit=0;

volatile int ott::server::__sig_alrm=0;

volatile int ott::server::__sig_chld=0;

void ott::server::__sig_handler(int n)
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

void ott::server::reset_signals(void)
    { common::reset_signals(); }

std::string ott::server::get_best_interface(void)
{
    std::string interface;

    struct network { in_addr_t addr; in_addr_t mask; };

    network networks[3];

    networks[0].addr=inet_addr("10.0.0.0");    networks[0].mask=inet_addr("255.0.0.0");
    networks[1].addr=inet_addr("172.16.0.0");  networks[1].mask=inet_addr("255.240.0.0");
    networks[2].addr=inet_addr("192.168.0.0"); networks[2].mask=inet_addr("255.255.0.0");

    char buf[16384];

    ifconf ifc;
    ifc.ifc_len=sizeof(buf);
    ifc.ifc_buf=buf;

    int s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==-1)
        return interface;

    if(ioctl(s,SIOCGIFCONF,&ifc)==-1)
        { close(s); return interface; }

    close(s);

    if(!ifc.ifc_len)
        return interface;

    ifreq* ifr=ifc.ifc_req;

    for(int i=0;i<ifc.ifc_len;)
    {
#if defined(__FreeBSD__) || defined(__APPLE__)
        size_t len=IFNAMSIZ+ifr->ifr_addr.sa_len;
#else
        size_t len=sizeof(*ifr);
#endif
        if(!i)
            interface=ifr->ifr_name;

        if(ifr->ifr_addr.sa_family==AF_INET)
        {
            for(int j=0;j<sizeof(networks)/sizeof(*networks);j++)
            {
                if((((sockaddr_in*)&ifr->ifr_addr)->sin_addr.s_addr & networks[j].mask)==networks[j].addr)
                    { interface=ifr->ifr_name; return interface; }
            }
        }

        ifr=(ifreq*)((char*)ifr+len);

        i+=len;
    }

    return interface;
}

std::string ott::server::get_if_addr(const std::string& interface)
{
    if(inet_addr(interface.c_str())!=INADDR_NONE)
        return interface;

    ifreq ifr;

    int n=snprintf(ifr.ifr_name,IFNAMSIZ,"%s",interface.c_str());

    if(n==-1 || n>=IFNAMSIZ)
        return std::string();

    int s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==-1)
        return std::string();

    if(ioctl(s,SIOCGIFADDR,&ifr)==-1 || ifr.ifr_addr.sa_family!=AF_INET)
        { close(s); return std::string(); }

    close(s);

    return inet_ntoa(((sockaddr_in*)&ifr.ifr_addr)->sin_addr);
}

int ott::server::listen(const std::string& addr)
{
    std::string host,port;

    bool any=false;

    {
        std::string::size_type n=addr.find(':');

        if(n!=std::string::npos)
            { host=addr.substr(0,n); port=addr.substr(n+1); }
        else
            { host=get_best_interface(); any=true; port=addr; }
    }

    host=get_if_addr(host);

    if(host.empty())
        return -1;

    sockaddr_in sin;
    sin.sin_family=AF_INET;
    sin.sin_port=htons(atoi(port.c_str()));
    sin.sin_addr.s_addr=any?INADDR_ANY:inet_addr(host.c_str());

#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sin);
#endif

    if(sin.sin_port==0 || sin.sin_addr.s_addr==INADDR_NONE)
        return -1;

    int fd=::socket(PF_INET,SOCK_STREAM,0);

    if(fd==-1)
        return -1;

    { int reuse=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse)); }

    if(bind(fd,(sockaddr*)&sin,sizeof(sin)) || ::listen(fd,config::backlog))
        { close(fd); return -1; }

    fcntl(fd,F_SETFL,O_NONBLOCK);

    config::if_addr=host;

    config::www_location=host+":"+port;

    config::env["www_location"]=config::www_location;

    return fd;
}

bool ott::server::update_media(void)
{
    // system(...);

    playlist.scan(media);

    return true;
}

bool ott::server::doit(const std::string& addr,bool ssdp_reg)
{
    update_media();

    int fd=listen(addr);

    if(fd==-1)
        return false;

    int sv[2];

    if(socketpair(AF_UNIX,SOCK_DGRAM,0,sv))
        { close(fd); return false; }

    setsid();

    if(ssdp_reg)
    {
        pid_t pid=fork();

        if(!pid)
        {
            close(fd); close(sv[0]); close(sv[1]);

            {
                ott::ssdp ssdp;

                ssdp.loop();
            }

            exit(0);
        }
    }

    common::signal(SIGINT, __sig_handler);
    common::signal(SIGQUIT,__sig_handler);
    common::signal(SIGALRM,__sig_handler);
    common::signal(SIGTERM,__sig_handler);
    common::signal(SIGCHLD,__sig_handler);
    common::signal(SIGHUP, SIG_IGN);
    common::signal(SIGPIPE,SIG_IGN);
    common::signal(SIGUSR1,SIG_IGN);
    common::signal(SIGUSR2,SIG_IGN);

    sigset_t fullset,emptyset;

    sigfillset(&fullset); sigemptyset(&emptyset);

    sigprocmask(SIG_SETMASK,&fullset,NULL);

    while(!__sig_quit)
    {
        fd_set fdset; FD_ZERO(&fdset);

        FD_SET(fd,&fdset); FD_SET(sv[0],&fdset);

        int maxfd=max(fd,sv[0]);

        int rc=pselect(maxfd+1,&fdset,NULL,NULL,NULL,&emptyset);

        if(rc==-1)
        {
            if(errno==EINTR)
            {
                if(__sig_chld)
                {
                    pid_t pid; int status;

                    while((pid=wait4(-1,&status,WNOHANG,NULL))>0)
                        on_release(pid);

                    __sig_chld=0;
                }

                if(__sig_alrm)
                    { on_alarm(); __sig_alrm=0; }

                continue;
            }else
                break;
        }

        if(FD_ISSET(fd,&fdset))
        {
            int newfd;

            sockaddr_in sin; socklen_t sinlen=sizeof(sin);

            while((newfd=accept(fd,(sockaddr*)&sin,&sinlen))!=-1)
            {
                { int on=1; setsockopt(newfd,IPPROTO_TCP,TCP_NODELAY,(const char*)&on,sizeof(on)); }

                fcntl(newfd,F_SETFL,fcntl(newfd,F_GETFL) & (~O_NONBLOCK));      // используем дескриптор в блокируемом режиме

                pid_t pid=fork();

                if(!pid)
                {
                    reset_signals();

                    dup2(newfd,0); dup2(newfd,1); close(newfd);

                    ott::worker child(sv[1],config::timeout,playlist,inet_ntoa(sin.sin_addr));

                    child.doit();

                    exit(0);
                }

                close(newfd);
            }
        }

        if(FD_ISSET(sv[0],&fdset))
        {
            char buf[config::tmp_buf_size];

            ssize_t len;

            while((len=recv(sv[0],buf,sizeof(buf),MSG_DONTWAIT))>0)
            {
                char* p=strchr(buf,' ');

                if(p)
                    { *p=0; on_addref(atoi(buf),p+1); }
            }
        }
    }

    sigprocmask(SIG_SETMASK,&emptyset,NULL);

    common::signal(SIGTERM,SIG_IGN);

    common::signal(SIGCHLD,SIG_IGN);

    kill(0,SIGTERM);

    close(fd); close(sv[0]); close(sv[1]);

    for(std::map<std::string,stream>::const_iterator it=streams.begin();it!=streams.end();++it)
        rmdir(config::cache_path+it->first);

    return true;
}

void ott::server::rmdir(const std::string& path)
{
    char buf[256]; snprintf(buf,sizeof(buf),"rm -rf %s",path.c_str()); buf[sizeof(buf)-1]=0;

    system(buf);
}

void ott::server::on_alarm(void)
{
}

void ott::server::on_addref(int pid,const std::string& url)
{
    stream& s=streams[url];

    if(s.pid==0)
    {
        pid_t pid=fork();

        if(!pid)
        {
            reset_signals();

            ott::producer child(url,playlist);

            child.doit();

            exit(0);
        }else if(pid!=-1)
            { s.nref=0; s.url=url; s.pid=pid; }
        else
            { streams.erase(url); return; }     // не получилось запустить
    }

    s.nref++;

    clients[pid]=&s;
}

void ott::server::on_release(int pid)
{
    std::map<pid_t,stream*>::iterator it=clients.find(pid);

    if(it!=clients.end())
    {
        stream* s=it->second;

        if(s)
        {
            s->nref--;

            if(s->nref<1)
            {
                if(s->pid)
                    { kill(s->pid,SIGTERM); kill(s->pid,SIGKILL); }

                rmdir(config::cache_path+s->url);

                streams.erase(s->url);
            }
        }

        clients.erase(it);
    }
}
