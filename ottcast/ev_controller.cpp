/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "defaults.h"
#include "ev_controller.h"
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include "plugin_broadcast.h"
#include "plugin_udp.h"
#include "plugin_hls.h"

namespace ev
{
    int timeout=0;

    std::string interface;

    std::string dlna_extras;

    std::string broadcast_cmd;

    int cache_size=0;

    std::string get_server_date(void);
}

std::string ev::get_server_date(void)
{
    time_t timestamp=time(NULL);

    tm* t=gmtime(&timestamp);

    char buf[128];

    int n=strftime(buf,sizeof(buf),"Date: %a, %d %b %Y %H:%M:%S GMT\r\n",t);

    return std::string(buf,n);
}

bool ev::controller::listen(const std::string& addr,int backlog)
{
    std::string host,port;

    {
        std::string::size_type n=addr.find(':');

        if(n!=std::string::npos)
            { host=addr.substr(0,n); port=addr.substr(n+1); }
        else
            port=addr;
    }

    sockaddr_in sin;
    sin.sin_family=AF_INET;
    sin.sin_port=htons(atoi(port.c_str()));
    sin.sin_addr.s_addr=host.empty()?INADDR_ANY:inet_addr(host.c_str());

    if(sin.sin_port==0 || sin.sin_addr.s_addr==INADDR_NONE)
        return false;

    int fd=::socket(PF_INET,SOCK_STREAM,0);

    { int reuse=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse)); }

    if(fd==-1)
        return false;

    if(bind(fd,(sockaddr*)&sin,sizeof(sin)) || ::listen(fd,backlog))
        { close(fd); return false; }

    fcntl(fd,F_SETFL,O_NONBLOCK);

    listener& l=listeners[fd];

    l.fd=fd;

    if(!eng->set(&l))
        { listeners.erase(fd); return false; }

    return true;
}

ev::source* ev::controller::spawn(const std::string& url)
{
    if(url.empty() || url[0]!='/')
        return NULL;

    m3u8::channel* chnl=NULL;

    {
        // /0001c0134.ts

        std::string::size_type n=url.find('.');

        if(n==std::string::npos || strcmp(url.c_str()+n,".ts"))
            return NULL;

        chnl=content.find(url.substr(1,n-1));
    }

    if(!chnl)
        return NULL;

    // запуск нового дочернего процесса - источника

    int pipefd[2];

    if(pipe(pipefd))
        return NULL;

    pid_t pid=fork();

    if(pid==-1)
        { close(pipefd[0]); close(pipefd[1]); return NULL; }

    if(!pid)
    {
        int fd=open("/dev/null",O_RDWR);

        if(fd!=-1)
            { dup2(fd,0); /*dup2(fd,2);*/ close(fd); }
        else
            { close(0); /*close(2);*/ }

        dup2(pipefd[1],1);

        close(pipefd[0]); close(pipefd[1]);

        signal(SIGHUP,SIG_DFL);
        signal(SIGINT,SIG_DFL);
        signal(SIGQUIT,SIG_DFL);
        signal(SIGPIPE,SIG_DFL);
        signal(SIGALRM,SIG_DFL);
        signal(SIGTERM,SIG_DFL);
        signal(SIGUSR1,SIG_DFL);
        signal(SIGUSR2,SIG_DFL);
        signal(SIGCHLD,SIG_DFL);

        sigset_t emptyset; sigemptyset(&emptyset);

        sigprocmask(SIG_SETMASK,&emptyset,NULL);

        source_main(*chnl);

        _exit(0);
    }

    close(pipefd[1]);

    int fd=pipefd[0];

    fcntl(fd,F_SETFL,O_NONBLOCK);

    source& s=sources[fd];

    s.fd=fd;

    if(!eng->set(&s))
        { sources.erase(s.fd); return NULL; }

    s.set_buf_size(chnl->cache>0?chnl->cache:cache_size);           // если ошибка - нет памяти, все грохнится!

    s.pid=pid;

    s.url=url;

    urls[url]=&s;

//    fprintf(stderr,"url='%s', cache=%i\n",s.url.c_str(),s.size);

    return &s;
}

void ev::controller::attach(client* c,source* s)
{
    c->src=s;

    s->clients.insert(c);

//    c->rd_offset=0;                                                             // минимально возможная стартовая позиция
//    c->rd_offset=defaults::ts_align_left(s->wr_offset);                                       // ближайшая кратная 188 позиция к указателю записи

/*
    // стартуем с предзаполненной половины буфера, либо с ближайшей кратной 188 позиции (например, с нуля)
    int half=s->size/2;

    if(s->wr_offset>=half)
        c->rd_offset=s->wr_offset-half;
    else
    {
        if(!s->rotate)
            c->rd_offset=0;
        else
            c->rd_offset=s->size-(half-s->wr_offset);
    }

    c->rd_offset=defaults::ts_align_left(c->rd_offset);
*/

    // стартуем с самого начала буфера - 100% заполнение (или меньше) с самого начала
    if(!s->rotate)
        c->rd_offset=0;
    else
    {
        c->rd_offset=defaults::ts_align_right(s->wr_offset+1);

        if(c->rd_offset>=s->size)
            c->rd_offset=0;
    }
}

bool ev::controller::join(client* c,const std::string& url)
{
    if(url.empty())
        return false;

    std::map<std::string,source*>::iterator it=urls.find(url);

    if(it!=urls.end())
        { attach(c,it->second); return true; }

    source* s=spawn(url);

    if(!s)
        return false;

    attach(c,s);

    return true;
}

void ev::controller::flush(void)
{
    for(std::list<client*>::iterator it=trash_clnt.begin();it!=trash_clnt.end();++it)
    {
        client* c=*it;

        source* s=c->src;

        eng->del(c);

        timers.remove(c);

        if(s)
        {
            s->clients.erase(c);

            if(s->clients.empty())
                trash_src.insert(s);
        }

        clients.erase(c->fd);
    }

    for(std::set<source*>::iterator it=trash_src.begin();it!=trash_src.end();++it)
    {
        source* s=*it;

        eng->del(s);

        for(std::set<client*>::iterator i=s->clients.begin();i!=s->clients.end();++i)
        {
            client* c=*i;

            eng->del(c);

            timers.remove(c);

            clients.erase(c->fd);
        }

        if(s->pid>0)
            kill(s->pid,SIGTERM);

        if(!s->url.empty())
            urls.erase(s->url);

        sources.erase(s->fd);
    }

    trash_clnt.clear();

    trash_src.clear();
}

void ev::controller::on_chld(pid_t pid)
{
}

void ev::controller::on_alrm(void)
{
    time_t now=time(NULL);

    client* c;

    while((c=timers.fetch(now)))
        trash_clnt.push_back(c);

    alarm(timers.get_wake_delay(now));
}

std::string ev::controller::get_url(const std::string& buf,bool& is_head)
{
    int offset=0;

    if(!strncasecmp(buf.c_str(),"GET ",4))
        { offset=4; }
    else if(!strncasecmp(buf.c_str(),"HEAD ",5))
        { is_head=true; offset=5; }

    if(offset>0)
    {
        const char* p;

        for(p=buf.c_str()+offset;*p==' ';p++);

        const char* p2=strchr(p,' ');

        if(p2)
        {
            std::string req(p,p2-p);

            while(*p2==' ') p2++;

            if(!strncasecmp(p2,"HTTP/1.",7))
                return req;
        }
    }

    return std::string();
}

void ev::controller::flush_buffer(client* c)
{
    int n;

    int fd=c->fd;

    const char* buf=c->src->buf;

    int size=c->src->size;

    int& rd_offset=c->rd_offset;

    int& wr_offset=c->src->wr_offset;

    while((n=write(fd,buf+rd_offset,(rd_offset>wr_offset)?(size-rd_offset):(wr_offset-rd_offset)))>0)
    {
        rd_offset+=n;

        if(rd_offset>=size)
            rd_offset=0;
    }

    if(n==-1 && (errno==EAGAIN || errno==EWOULDBLOCK))
        { c->is_pollout=false; eng->set(c); }
}

void ev::controller::on_event(socket* s)
{
//printf("%i %i,%i,%i\n",s->fd,s->is_pollin,s->is_pollout,s->is_pollhup);

    static char buf[2048];

    bool to_trash=false;

    // серверный сокет
    if(s->type==types::listener)
    {
        if(s->is_pollin)
        {
            time_t now=time(NULL);

            int fd;

            while((fd=::accept(s->fd,NULL,NULL))!=-1)
            {
                if(defaults::max_clients>0 && clients.size()>=defaults::max_clients)
                    close(fd);
                else
                {
                    fcntl(fd,F_SETFL,O_NONBLOCK);

                    int on=1;

                    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,(const char*)&on,sizeof(on));

//                    setsockopt(fd,IPPROTO_TCP,TCP_CORK,(const char*)&on,sizeof(on));

                    client& c=clients[fd];

                    c.fd=fd;

                    if(!eng->set(&c))
                        clients.erase(c.fd);
                    else
                        timers.insert(&c,now+timeout);
                }
            }

            s->is_pollin=false;

            alarm(timers.get_wake_delay(now));
        }

        eng->set(s);

    // клиентский сокет
    }else if(s->type==types::client)
    {
        client& c=*((client*)s);

        int n;

        if(c.is_pollhup)
            to_trash=true;

        if(!to_trash && c.is_pollin)
        {
            while((n=read(c.fd,buf,sizeof(buf)))>0)
            {
                if(c.st<10)
                {
                    for(int i=0;i<n;i++)
                    {
                        int ch=((unsigned char*)buf)[i];

                        if(ch!='\r')
                        {
                            switch(c.st)
                            {
                            case 0: if(ch==' ') continue; else { c.buf+=ch; c.st=1; } break;
                            case 1: if(ch!='\n') { if(c.buf.length()<defaults::max_req_len) c.buf+=ch; } else c.st=3; break;
                            case 2: if(ch=='\n') c.st=3; break;
                            case 3:
                                if(ch!='\n')
                                    c.st=2;
                                else
                                {
                                    c.st=10;    // 10 - вывести ответ и закрыть соединение, 11 - вывести ответ и стрим

                                    bool is_head=false;

                                    std::string s=get_url(c.buf,is_head);

                                    c.buf.assign("HTTP/1.1 ");

                                    if(s.empty())
                                        c.buf.append("400 Bad Request\r\n");
                                    else if(!is_head && !join(&c,s))
                                        c.buf.append("403 Forbidden\r\n");
                                    else
                                        { c.buf.append("200 OK\r\n"); c.st=11; }

                                    c.buf.append("Server: xupnpd-live\r\n");
                                    c.buf.append(get_server_date());
                                    c.buf.append("Connection: close\r\n");

                                    if(c.st==11 && !dlna_extras.empty())
                                        { c.buf.append(dlna_extras); c.buf.append("\r\n"); }

                                    c.buf.append("\r\n");

                                    if(c.st==11 && is_head)
                                        c.st=10;
                                }

                                break;
                            }
                        }
                    }
                }
            }

            if(n==-1)
            {
                if(errno==EAGAIN || errno==EWOULDBLOCK)
                    c.is_pollin=false;
                else
                    to_trash=true;
            }else
                to_trash=true;
        }

        if(!to_trash && c.is_pollout)
        {
            if(c.st==10 || c.st==11)    // выдать ответ из buf
            {
                while((n=write(c.fd,c.buf.c_str()+c.snd_offset,c.buf.length()-c.snd_offset))>0)
                    c.snd_offset+=n;

                if(n==-1)
                {
                    if(errno==EAGAIN || errno==EWOULDBLOCK)
                        c.is_pollout=false;
                    else
                        to_trash=true;
                }else
                {
                    if(c.st==10)
                        to_trash=true;
                    else
                        { timers.remove(&c); c.st=20; c.buf.clear(); } // сброс таймера, теперь ограничений по времени нет
                }
            }

            if(!to_trash && c.is_pollout && c.st==20)   // стриминг
                flush_buffer(&c);
        }

        if(to_trash)
            trash_clnt.push_back(&c);
        else
            eng->set(s);

    // источник
    }else if(s->type==types::source)
    {
        source& c=*((source*)s);

        int n;

        if(c.is_pollhup)
            to_trash=true;

        if(!to_trash && c.is_pollin)
        {
            int bytes_read=0;

            while((n=read(c.fd,c.buf+c.wr_offset,c.size-c.wr_offset))>0)    // стриминг
            {
                bytes_read+=n;

                if(bytes_read>=c.size) {}               // произошло переполнение буфера, нужно скинуть обороты
                                                        // думаю надо прервать цикл чтения и дать клиентам возможность забрать  данные

                c.wr_offset+=n;

                if(c.wr_offset>=c.size)
                    { c.wr_offset=0; c.rotate=true; }
            }

            if(n==-1)
            {
                if(errno==EAGAIN || errno==EWOULDBLOCK)
                    c.is_pollin=false;
                else
                    to_trash=true;
            }else
                to_trash=true;


            for(std::set<client*>::iterator it=c.clients.begin();it!=c.clients.end();++it)
            {
                client& cc=**it;

                if(cc.is_pollout && cc.st==20)
                    flush_buffer(&cc);
            }
        }

        if(to_trash)
            trash_src.insert(&c);
        else
            eng->set(s);
    }
}

// выполняется в дочернем процессе
void ev::controller::source_main(const m3u8::channel& chnl)
{
    const std::string& source=chnl.parent->source;

    const std::string& ifname=chnl.parent->interface;

    if(source=="hls")
        hls::doit(chnl.url,chnl.stream,chnl.parent->via,chnl.parent->retry,chnl.parent->proxy);
    else if(source=="http")
        http::doit(chnl.url,chnl.parent->via,chnl.parent->proxy);
    else if(source=="udp")
        udp::doit(chnl.url,ifname.empty()?interface:ifname);
    else if(source=="broadcast")
        broadcast::doit(chnl.url,broadcast_cmd,chnl.loop);
    else
    {
        char buf[512];

        if(snprintf(buf,sizeof(buf),"%s '%s'",source.c_str(),chnl.url.c_str())<sizeof(buf))
            int rc=system(buf);
    }
}

