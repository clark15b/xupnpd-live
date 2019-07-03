/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "defaults.h"
#include "curl.h"
#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdlib.h>
#include <stdio.h>

namespace curl
{
    class connection
    {
    public:
        int fd;

        sockaddr_in sin;

        char buf[defaults::tcp_buf_size];

        int size;

        int offset;
    public:
        connection(void):fd(-1),size(0),offset(0) { sin.sin_port=0; sin.sin_addr.s_addr=0; }

        ~connection(void) { disconnect(); }

        bool eof(void) { return fd==-1?true:false; }

        bool lookup(const std::string& s);

        void disconnect(void);

        bool write(const char* p,int len);

        int gets(char* p,int len);

        int read(char** p,int len);
    };

    std::map<std::string,connection> connections;

    ssize_t recv(int fd,char* buf,size_t len,int flags);

    connection* connect(const std::string& s);
}

bool curl::connection::lookup(const std::string& s)
{
    if(sin.sin_port!=0 && sin.sin_addr.s_addr!=0)
        return true;

    std::string host; int port=0;

    std::string::size_type n=s.find(':');

    if(n!=std::string::npos)
        { host=s.substr(0,n); port=atoi(s.c_str()+n+1); }
    else
        { host=s; port=80; }

    in_addr_t addr=inet_addr(host.c_str());

    if(addr==INADDR_NONE)
    {
        hostent* he=gethostbyname(host.c_str());

        if(he)
            memcpy((char*)&addr,he->h_addr,sizeof(addr));
    }

    if(port==0 || addr==INADDR_NONE)
        return NULL;

    sin.sin_family=AF_INET;
    sin.sin_port=htons(port);
    sin.sin_addr.s_addr=addr;

    return &sin;
}

void curl::connection::disconnect(void)
{
    if(fd!=-1)
        { ::close(fd); fd=-1; }
}

bool curl::connection::write(const char* p,int len)
{
    if(len==-1)
        len=strlen(p);

    int bytes_sent=0;

    while(bytes_sent<len)
    {
        ssize_t n=::send(fd,p+bytes_sent,len-bytes_sent,0);

        if(n==-1 || n==0)
            { disconnect(); return false; }

        bytes_sent+=n;
    }

    return true;
}

ssize_t curl::recv(int fd,char* buf,size_t len,int flags)
{
    fd_set fdset; FD_ZERO(&fdset); FD_SET(fd,&fdset);

    timeval tv;
    tv.tv_sec=defaults::tcp_timeout;
    tv.tv_usec=0;

    int n=select(fd+1,&fdset,NULL,NULL,&tv);

    if(n==-1 || n==0 || !FD_ISSET(fd,&fdset))
        return -1;

    return ::recv(fd,buf,len,flags);
}

int curl::connection::gets(char* p,int len)
{
    int bytes_read=0;

    bool stop=false;

    while(!stop)
    {
        if(offset>=size)
        {
            ssize_t n=curl::recv(fd,buf,sizeof(buf),0);

            if(n==-1 || n==0)
                { disconnect(); return 0; }

            size=n; offset=0;
        }

        while(offset<size)
        {
            int ch=((unsigned char*)buf)[offset++];

            if(ch=='\n')
                { stop=true; break; }
            else if(ch!='\r' && bytes_read<len-1)
                ((unsigned char*)p)[bytes_read++]=ch;
        }
    }

    p[bytes_read]=0;

    return bytes_read;
}

int curl::connection::read(char** p,int len)
{
    if(!len)
        len=sizeof(buf);

    if(offset>=size)
    {
        ssize_t n=curl::recv(fd,buf,sizeof(buf)>len?len:sizeof(buf),0);

        if(n==-1 || n==0)
            { disconnect(); return 0; }

        size=n; offset=0;
    }

    *p=buf+offset;

    int rc=size-offset;

    if(len<rc)
        rc=len;

    offset+=rc;

    return rc;
}

curl::connection* curl::connect(const std::string& s)
{
    connection& con=connections[s];

    if(con.fd!=-1)              // проверка соединения
    {
        char buf[8];

        ssize_t n=::recv(con.fd,buf,sizeof(buf),MSG_DONTWAIT|MSG_PEEK);

        if(n==0 || (n==-1 && errno!=EAGAIN && errno!=EWOULDBLOCK))
            con.disconnect();
    }

    if(con.fd==-1)
    {
        if(!con.lookup(s))
            return NULL;

        con.fd=socket(PF_INET,SOCK_STREAM,0);

        if(con.fd==-1)
            return NULL;

        bool connected=false;

        for(int i=0;i<defaults::tcp_connect_retries;i++)
        {
            if(!::connect(con.fd,(sockaddr*)&con.sin,sizeof(con.sin)))
                { connected=true; break; }

            usleep(defaults::tcp_connect_retry_delay*1000);
        }
//printf("CONNECT\n");
        if(!connected)
            { con.disconnect(); return NULL; }
    }

    return &con;
}

bool curl::req::open(const std::string& s)
{
    if(s.empty())
        return false;

    static const char http_prefix[]="http://";

    static const int http_prefix_size=sizeof(http_prefix)-1;

    if(strncmp(s.c_str(),http_prefix,http_prefix_size))
        return false;

    std::string::size_type n=s.find('/',http_prefix_size);

    if(n!=std::string::npos)
        { host_name=s.substr(http_prefix_size,n-http_prefix_size); url=s.substr(n); }
    else
        { host_name=s.substr(http_prefix_size); url="/"; }

    n=url.find_last_of('/');

    if(n!=std::string::npos)
        path=url.substr(0,n+1);

    return true;
}

bool curl::req::perform_real(void)
{
    ret_code=0; content_length=0;

    curl::connection* con=curl::connect(host_name);

    if(!con)
        return false;

    int length=-1;

    int n;

    {
        char buf[512];

        n=snprintf(buf,sizeof(buf),"GET %s HTTP/1.0\r\nHost: %s\r\nConnection: Keep-Alive\r\n\r\n",
            url.c_str(),host_name.c_str());

        if(n==-1 || n>=sizeof(buf) || !con->write(buf,n))
            { con->disconnect(); return false; }

        int idx=0;

        while((n=con->gets(buf,sizeof(buf)))>0)
        {
            if(!idx++)
            {
                if(!strncasecmp(buf,"HTTP/1.0 ",9) || !strncasecmp(buf,"HTTP/1.1 ",9))
                {
                    char* p; for(p=buf+9;*p==' ';p++);

                    char* p2=strchr(p,' ');

                    if(p2)
                        *p2=0;

                    ret_code=atoi(p);
                }
            }else
            {
                if(length==-1 && !strncasecmp(buf,"Content-Length:",15))
                    content_length=length=atoi(buf+15);

                if(location.empty() && !strncasecmp(buf,"Location:",9))
                    { char* p; for(p=buf+9;*p==' ';p++); location.assign(p); }
            }
        }
    }

    if(ret_code!=200)
        { con->disconnect(); return false; }

    if(con->eof())
        return false;

    char* buf;

    if(length==-1)
    {
        while((n=con->read(&buf,0))>0)
            on_data(buf,n);

        con->disconnect();

    }else
    {
        while(length>0 && (n=con->read(&buf,length))>0)
        {
            on_data(buf,n);

            length-=n;
        }

        if(length>0)
            { con->disconnect(); return false; }
    }

    return true;
}

bool curl::req::perform(void)
{
    bool rc=false;

    for(int i=0;i<5 && !(rc=perform_real());i++)
    {
        if(location.empty() || !open(location))
            break;

        location.clear();
    }

    return rc;
}

/*
int main(void)
{
    for(int i=0;i<10;i++)
    {
        curl::req req("http://62.113.210.250/medienasa-live/_definst_/mp4:tvhalle_high/playlist.m3u8");

        req.perform();
    }

    return 0;
}
*/
