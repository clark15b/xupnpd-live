#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "config.h"

namespace logger
{
    int fd=-1;

    std::string pri;    // (facility<<3)+level - 134

    std::string ident="none";
}

void logger::open(const std::string& s)
{
    close();

    if(s.empty())
        return;

    if(!strcasecmp(s.c_str(),"stdout"))
        fd=1;
    else if(!strcasecmp(s.c_str(),"stderr"))
        fd=2;
    else
    {
        std::string ss;

        std::string::size_type n=s.find('/');

        if(n!=std::string::npos)
            { ss=s.substr(0,n); pri=s.substr(n+1); }
        else
            { ss=s; pri="0"; }

        sockaddr_in sin;

        sin.sin_family=AF_INET;

        n=ss.find(':');

        if(n!=std::string::npos)
            { sin.sin_addr.s_addr=inet_addr(ss.substr(0,n).c_str()); sin.sin_port=htons(atoi(ss.substr(n+1).c_str())); }
        else
            { sin.sin_addr.s_addr=inet_addr(ss.c_str()); sin.sin_port=htons(514); }

        fd=socket(PF_INET,SOCK_DGRAM,0);

        if(fd==-1)
            return;

        if(connect(fd,(sockaddr*)&sin,sizeof(sin)))
            close();
    }
}

void logger::close(void)
{
    if(fd>2)
        { ::close(fd); fd=-1; }
}

void logger::print(const char* fmt,...)
{
    if(fd==-1)
        return;

    char buf[config::tmp_buf_size];

    int offset=0; int size=sizeof(buf);

    if(fd>2)
    {
        // RFC3164
        // <134>Oct 23 18:33:43 l560 shocker: Hello3

        time_t timestamp=time(NULL);

        tm* t=localtime(&timestamp);

        static const char* mon[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

        offset=sprintf(buf,"<%s>%s %.2i %.2i:%.2i:%.2i %s: ",
            pri.c_str(),mon[t->tm_mon],t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec,ident.c_str());

        size-=offset;
    }

    va_list ap;

    va_start(ap,fmt);

    int n=vsnprintf(buf+offset,size,fmt,ap);

    va_end(ap);

    if(n==-1 || n>=size)
        n=size-1;

    if(fd<3)
        { buf[n]='\n'; write(fd,buf,n+offset+1); }
    else
        send(fd,buf,n+offset,MSG_DONTWAIT);
}
