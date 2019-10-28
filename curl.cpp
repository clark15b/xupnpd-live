/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "curl.h"
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include "log.h"
#include "common.h"

namespace curl
{
    inline u_int64_t pack(const sockaddr_in* sin)
        { return (((u_int64_t)sin->sin_addr.s_addr)<<16) | sin->sin_port; }

    context* context::self=NULL;
}

curl::context::context(void)
{
    self=this;

#ifdef WITH_SSL
    static const char custom_seed[]="VerYvEryVErYSEcRetwORd";

    mbedtls_ssl_config_init(&conf);

    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_entropy_init(&entropy);

    mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,(const unsigned char*)custom_seed,sizeof(custom_seed)-1);

    mbedtls_ssl_config_defaults(&conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);

    mbedtls_ssl_conf_rng(&conf,mbedtls_ctr_drbg_random,&ctr_drbg);

    mbedtls_ssl_conf_authmode(&conf,MBEDTLS_SSL_VERIFY_NONE);
#endif /* WITH_SSL */
}

curl::context::~context(void)
{
    connections.clear();

#ifdef WITH_SSL
    mbedtls_entropy_free(&entropy);

    mbedtls_ctr_drbg_free(&ctr_drbg);

    mbedtls_ssl_config_free(&conf);
#endif /* WITH_SSL */
}

curl::connection::connection(void):fd(-1),id(0),size(0),offset(0),https(false)
{
}

curl::connection::~connection(void)
{
#ifdef WITH_SSL
    if(https)
        ssl_disconnect();
    else
#endif /* WITH_SSL */

    if(fd!=-1)
        close(fd);
}

#ifdef WITH_SSL
bool curl::connection::ssl_connect(void)
{
    https=true;

    mbedtls_net_init(&server_fd);

    mbedtls_ssl_init(&ssl);

    mbedtls_ssl_setup(&ssl,&context::get().conf);

    server_fd.fd=fd;

    fcntl(server_fd.fd,F_SETFL,O_NONBLOCK);

    mbedtls_ssl_set_bio(&ssl,&server_fd,mbedtls_net_send,mbedtls_net_recv,NULL);

    for(int i=0;i<10;i++)
    {
        fd_set fdset; FD_ZERO(&fdset); FD_SET(server_fd.fd,&fdset);

        timeval tv; tv.tv_sec=config::tcp_timeout; tv.tv_usec=0;

        int rc=-1;

        switch(mbedtls_ssl_handshake(&ssl))
        {
        case 0:
            return true;
        case MBEDTLS_ERR_SSL_WANT_READ:
            rc=select(server_fd.fd+1,&fdset,NULL,NULL,&tv);
            break;
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            rc=select(server_fd.fd+1,NULL,&fdset,NULL,&tv);
            break;
        default:
            return false;
        }

        if(rc!=1)
            break;
    }

    return false;
}

bool curl::connection::ssl_disconnect(void)
{
    mbedtls_ssl_free(&ssl);

    mbedtls_net_free(&server_fd);

    return true;
}

ssize_t curl::connection::ssl_recv(int fd,char* buf,size_t len)
{
    for(int i=0;i<50;i++)
    {
        int rc=mbedtls_ssl_read(&ssl,(unsigned char*)buf,len);

        if(rc>0)
            return rc;

        fd_set fdset; FD_ZERO(&fdset); FD_SET(server_fd.fd,&fdset);

        timeval tv; tv.tv_sec=config::tcp_timeout; tv.tv_usec=0;

        switch(rc)
        {
        case MBEDTLS_ERR_SSL_WANT_READ:
            rc=select(server_fd.fd+1,&fdset,NULL,NULL,&tv);
            break;
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            rc=select(server_fd.fd+1,NULL,&fdset,NULL,&tv);
            break;
        default:
            return -1;
        }

        if(rc!=1)
            break;
    }

    return -1;
}

ssize_t curl::connection::ssl_send(int fd,const char* buf,size_t len)
{
    for(int i=0;i<50;i++)
    {
        int rc=mbedtls_ssl_write(&ssl,(const unsigned char*)buf,len);

        if(rc>0)
            return rc;

        fd_set fdset; FD_ZERO(&fdset); FD_SET(server_fd.fd,&fdset);

        timeval tv; tv.tv_sec=config::tcp_timeout; tv.tv_usec=0;

        switch(rc)
        {
        case MBEDTLS_ERR_SSL_WANT_READ:
            rc=select(server_fd.fd+1,&fdset,NULL,NULL,&tv);
            break;
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            rc=select(server_fd.fd+1,NULL,&fdset,NULL,&tv);
            break;
        default:
            return -1;
        }

        if(rc!=1)
            break;
    }

    return -1;
}
#endif /* WITH_SSL */

ssize_t curl::connection::direct_recv(int fd,char* buf,size_t len)
{
    fd_set fdset; FD_ZERO(&fdset); FD_SET(fd,&fdset);

    timeval tv; tv.tv_sec=config::tcp_timeout; tv.tv_usec=0;

    int n=select(fd+1,&fdset,NULL,NULL,&tv);

    if(n==-1 || n==0 || !FD_ISSET(fd,&fdset))
        return -1;

    return ::recv(fd,buf,len,0);
}

ssize_t curl::connection::direct_send(int fd,const char* buf,size_t len)
{
    fd_set fdset; FD_ZERO(&fdset); FD_SET(fd,&fdset);

    timeval tv; tv.tv_sec=config::tcp_timeout; tv.tv_usec=0;

    int n=select(fd+1,NULL,&fdset,NULL,&tv);

    if(n==-1 || n==0 || !FD_ISSET(fd,&fdset))
        return -1;

    return ::send(fd,buf,len,0);
}

bool curl::connection::send(const std::string& s)
{
    int bytes_sent=0;

    while(bytes_sent<s.length())
    {
        ssize_t n=send(fd,s.c_str()+bytes_sent,s.length()-bytes_sent);

        if(n==-1 || n==0)
            return false;

        bytes_sent+=n;
    }

    return true;
}

int curl::connection::recv(char** p,int len)
{
    if(!len)
        len=sizeof(buf);

    if(offset>=size)
    {
        ssize_t n=recv(fd,buf,sizeof(buf)>len?len:sizeof(buf));

        if(n==-1 || n==0)
            return -1;

        size=n; offset=0;
    }

    *p=buf+offset;

    int rc=size-offset;

    if(len<rc)
        rc=len;

    offset+=rc;

    return rc;
}

bool curl::connection::recvln(std::string& s)
{
    s.clear();

    for(;;)
    {
        if(offset>=size)
        {
            ssize_t n=recv(fd,buf,sizeof(buf));

            if(n==-1 || n==0)
                break;

            size=n; offset=0;
        }

        while(offset<size)
        {
            int ch=((unsigned char*)buf)[offset++];

            if(ch=='\n')
                return true;
            else if(ch!='\r' && s.length()<config::header_length_max)
                s+=ch;
        }
    }

    return false;
}

bool curl::context::lookup(const std::string& s,sockaddr_in* sin,bool https)
{
    std::string host; int port=0;

    std::string::size_type n=s.find(':');

    if(n!=std::string::npos)
        { host=s.substr(0,n); port=atoi(s.c_str()+n+1); }
    else
        { host=s; port=https?443:80; }

    if(host.empty() || port==0)
        return false;

    sin->sin_family=AF_INET;
    sin->sin_port=htons(port);

#if defined(__FreeBSD__) || defined(__APPLE__)
    sin->sin_len=sizeof(sockaddr_in);
#endif

    std::map<std::string,in_addr_t>::const_iterator it=hosts.find(host);

    if(it!=hosts.end())
        { sin->sin_addr.s_addr=it->second; return true; }

    sin->sin_addr.s_addr=common::lookup(host.c_str(),config::tcp_lookup_timeout);

    if(sin->sin_addr.s_addr==INADDR_NONE)
        return false;

    hosts[host]=sin->sin_addr.s_addr;

    return true;
}

curl::connection* curl::context::connect(const sockaddr_in* sin,bool https)
{
    u_int64_t id=pack(sin);

    std::map<u_int64_t,connection>::iterator it=connections.find(id);

    if(it!=connections.end())
    {
        if(connected(it->second.fd))
            return &it->second;
        else
            connections.erase(it);
    }

    int fd=common::connect(sin,config::tcp_connect_timeout);

    if(fd==-1)
        return NULL;

    connection& c=connections[id];

    c.fd=fd;

    c.id=id;

#ifdef WITH_SSL
    if(https && !c.ssl_connect())
        { connections.erase(id); return NULL; }
#endif /* WITH_SSL */

    return &c;
}

bool curl::connected(int fd)    // нет смысла с SSL т.е в буфере может быть close notify, а мы подумаем что сокет живой
{
    if(fd==-1)
        return false;

    char tmp[32];

    int n=::recv(fd,tmp,sizeof(tmp),MSG_PEEK | MSG_DONTWAIT);

    if(n>0 || (n==-1 && (errno==EAGAIN || errno==EWOULDBLOCK)))
        return true;

    return false;
}

std::string curl::trim(const std::string& s,bool toupr)
{
    std::string::size_type p1=s.find_first_not_of(' ');

    if(p1!=std::string::npos)
    {
        std::string::size_type p2=s.length();

        while(p2>0 && s[p2-1]==' ')
            --p2;

        if(toupr)
        {
            std::string ss; ss.reserve(p2-p1);

            for(std::string::size_type i=p1;i<p2;++i)
                ss+=(s[i]=='-')?'_':toupper(s[i]);

            return ss;
        }else
            return s.substr(p1,p2-p1);
    }

    return std::string();
}

bool curl::req::open(const std::string& s)
{
    if(s.empty())
        return false;

    std::string::size_type prefix_size=s.find("://");

    if(prefix_size==std::string::npos)
        return false;

#ifdef WITH_SSL
    https=(s.substr(0,prefix_size)=="https")?true:false;
#endif /* WITH_SSL */

    prefix_size+=3;

    std::string::size_type n=s.find('/',prefix_size);

    if(n!=std::string::npos)
        { host_name=s.substr(prefix_size,n-prefix_size); url=s.substr(n); }
    else
        { host_name=s.substr(prefix_size); url="/"; }

    n=url.find_last_of('/');

    if(n!=std::string::npos)
        path=url.substr(0,n+1);

    return true;
}

bool curl::req::perform_real(connection* c,const std::string& post_data,const std::list<std::string>& hdrs)
{
    ret_code=0; keep_alive=false; content_length=-1; headers.clear(); on_clear();

    // отправка запроса
    {
        std::string s=post_data.empty()?"GET ":"POST "; s+=url; s+=" HTTP/1.1\r\n";
        s+="User-Agent: "; s+=config::user_agent; s+="\r\n";
        s+="Accept: */*\r\n";
        s+="Accept-Encoding: identity\r\n";
        s+="Host: "; s+=host_name; s+="\r\n";
        s+="Connection: Keep-Alive\r\n";

        if(!post_data.empty())
            { char buf[128]; int n=sprintf(buf,"Content-Length: %lu\r\n",(unsigned long)post_data.length()); s.append(buf,n); }

        for(std::list<std::string>::const_iterator it=hdrs.begin();it!=hdrs.end();++it)
            { s+=*it; s+="\r\n"; }

        s+="\r\n";

        if(!c->send(s))
            return false;

        if(!post_data.empty() && !c->send(post_data))
            return false;
    }

    std::string s;

    int idx=0;

    // чтение заголовков
    for(;;)
    {
        if(!c->recvln(s))
            return false;

        if(s.empty() && idx)
            break;

        if(!s.empty() && idx<config::headers_max)
        {
            ++idx;

            if(idx==1)
            {
                // разбор строки статуса

                std::string status_line=trim(s,false);

                std::string http_ver;

                std::string::size_type n=status_line.find(' ');

                if(n!=std::string::npos)
                {
                    http_ver=status_line.substr(0,n);

                    if(strncasecmp(http_ver.c_str(),"HTTP/",5))         // не HTTP!!!
                        return false;

                    for(;status_line[n]==' ';++n);

                    ret_code=strtol(status_line.substr(n).c_str(),NULL,10);

                    if(ret_code<1)                                      // не должно такого быть, скорее всего опять не HTTP
                        return false;
                }

                keep_alive=http_ver.substr(5)=="1.0"?false:true;
            }else
            {
                std::string::size_type n=s.find(':');

                if(n!=std::string::npos)
                    headers[trim(s.substr(0,n),true)]=trim(s.substr(n+1),false);
            }
        }
    }

    // чтение данных
    char* data; int n;

    std::map<std::string,std::string>::const_iterator it=headers.find("CONTENT_LENGTH");

    if(it!=headers.end())
    {
        // есть Content-Length
        long length=strtol(it->second.c_str(),NULL,10);

        if(length<0)
            return false;

        content_length=length;

        while(length>0 && (n=c->recv(&data,length))>0)
            { on_data(data,n); length-=n; }

        if(length>0)
            return false;
    }else
    {
        // нет Content-Length
        it=headers.find("TRANSFER_ENCODING");

        if(it==headers.end() || strcasecmp(it->second.c_str(),"chunked"))
        {
            // просто читаем до конца
            while((n=c->recv(&data,0))>0)
                on_data(data,n);
        }else
        {
            // мать его, Transfer-Encoding: chunked нарисовался

            long length=0;

            for(;;)
            {
                if(length<1)
                {
                    std::string s;

                    if(!c->recvln(s))
                        return false;

                    if(s.empty())
                        continue;

                    length=strtol(s.c_str(),NULL,16);

                    if(length<1)
                        break;
                }

                if((n=c->recv(&data,length))>0)
                    { on_data(data,n); length-=n; }
                else
                    break;
            }

            if(length>0)
                return false;
        }
    }

    it=headers.find("CONNECTION");

    if(it!=headers.end())
        keep_alive=strcasecmp(it->second.c_str(),"Keep-Alive")?false:true;

    return true;
}

bool curl::req::perform(const std::string& post_data,const std::list<std::string>& hdrs,bool one_shot)
{
    bool rc=false;

    for(int i=0;i<config::tcp_reconnect_attempts;i++)
    {
        rc=false;

        connection* c=ctx.open(host_name,https);

        if(c)
        {
            if(!perform_real(c,post_data,hdrs))
                ctx.close(c);
            else
            {
                rc=true;

                if(!keep_alive)
                    ctx.close(c);

                if((ret_code>=301 && ret_code<=303) || ret_code==305 || ret_code==307)
                {
                    const std::string location=headers["LOCATION"];

                    if(location.empty() || !open(location))
                        { rc=false; break; }

                    continue;
                }else if(!(ret_code>=500 && ret_code<600))
                    break;
            }
        }

        if(one_shot)
            break;

        usleep(config::tcp_reconnect_pause*1000);
    }

    if(!rc || ret_code!=200)
        logger::print("ret_code=%i [%c] \"%s%s\"",ret_code,rc?'+':'-',host_name.c_str(),url.c_str());

    return rc;
}

void curl::simple_req::on_data(const char* p,int len)
{
    long left=config::content_length_max-content.length();

    if(left>0)
        content.append(p,len>left?left:len);
}

/*
int main(void)
{
    curl::context ctx;

//    curl::simple_req req(ctx,"http://192.168.1.233:8008/ssdp/device-desc.xml");
//    curl::simple_req req(ctx,"http://192.168.1.175:1900/description.xml");
//    curl::simple_req req(ctx,"http://kineskop.tv/?page=watch&ch=256");

    curl::simple_req req(ctx,"https://www.wetter.com/hd-live-webcams/tschechien/prague/556ea720e8c1b/");

//    curl::simple_req req(ctx,"https://livecdn01-earthtv-com.freetls.fastly.net/stream12/cdnedge/vv170/playlist.m3u8?token=eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJhdWQiOiJodHRwczovL2xpdmVjbG91ZC5lYXJ0aHR2LmNvbSIsImV4cCI6MTU0Mjg5MzQ0NiwiaHR0cHM6Ly9saXZlY2xvdWQuZWFydGh0di5jb20iOnsiYWlkIjoiZTVmMmVkOGYtODNiYi00NGNmLWI4YjAtNDhmYTVmMjNjY2UwIiwic2lkIjoiMzJjYjczZTUtNmRjZi00NTMwLTkyYjQtZjU3ZmFkYTQ1MmUwIiwidGlkIjoiOTcyNmZiYmEtNzQ0Zi00N2Y5LWJmZTItMWY4ODA2NTIyMTZiIiwidHlwIjoidXJpIn0sImlhdCI6MTU0Mjg3NTQ0Nn0.CbuLvEGRDPTx5aObn2A1J5hnT_k70Q4Gv12NjvUbTAs");

    if(req.perform(std::string(),std::list<std::string>(),true))
    {
        printf("ret_code=%i\n",req.ret_code);

        for(std::map<std::string,std::string>::const_iterator it=req.headers.begin();it!=req.headers.end();++it)
            printf("'%s'='%s'\n",it->first.c_str(),it->second.c_str());

        printf("%s\n",req.content.c_str());
    }

    return 0;
}
*/
