/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "ssdp.h"
#include "common.h"
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include "config.h"
#include "log.h"

namespace ott
{
    const char* ssdp::service_list[]=
    {
        "upnp:rootdevice",
        "urn:schemas-upnp-org:device:MediaServer:1",
        "urn:schemas-upnp-org:service:ContentDirectory:1",
        "urn:schemas-upnp-org:service:ConnectionManager:1",
        "urn:microsoft.com:service:X_MS_MediaReceiverRegistrar:1",
        NULL
    };

    static const char ssdp_group_addr[]="239.255.255.250";

    static const int ssdp_group_port=1900;

    static const int ssdp_send_delay_usec=1000;
}

ott::ssdp::~ssdp(void)
{
    if(fd!=-1)
    {
        ip_mreq mcast_group;
        memset((char*)&mcast_group,0,sizeof(mcast_group));
        mcast_group.imr_multiaddr.s_addr=ssdp_sin.sin_addr.s_addr;
        mcast_group.imr_interface.s_addr=inet_addr(config::if_addr.c_str());

        setsockopt(fd,IPPROTO_IP,IP_DROP_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group));

        close(fd);
    }
}

void ott::ssdp::register_msg(std::list<std::string>& t,const std::string& nt,const std::string& usn,const std::string& nts)
{
    char buf[config::tmp_buf_size];

    int n=snprintf(buf,sizeof(buf),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: %s:%i\r\n"
        "CACHE-CONTROL: max-age=%i\r\n"
        "LOCATION: http://%s/upnp/device.xml\r\n"
        "NT: %s\r\n"
        "NTS: %s\r\n"
        "Server: %s\r\n"
        "USN: %s\r\n\r\n",
        ssdp_group_addr,ssdp_group_port,config::ssdp_max_age,config::www_location.c_str(),nt.c_str(),nts.c_str(),
            config::device_description.c_str(),usn.c_str());

    if(n==-1 || n>=sizeof(buf))
        n=sizeof(buf)-1;

    t.push_back(std::string()); t.back().assign(buf,n);
}

void ott::ssdp::register_services(std::list<std::string>& t,const std::string& nts)
{
    register_msg(t,config::uuid,config::uuid,nts);

    for(int i=0;service_list[i];i++)
        register_msg(t,service_list[i],config::uuid+"::"+service_list[i],nts);
}

bool ott::ssdp::send_announce(const std::list<std::string>& t)
{
    if(fd==-1)
        return false;

    for(std::list<std::string>::const_iterator it=t.begin();it!=t.end();++it)
    {
        if(it!=t.begin())
            usleep(ssdp_send_delay_usec);

        sendto(fd,it->c_str(),it->length(),0,(sockaddr*)&ssdp_sin,sizeof(ssdp_sin));
    }

    return true;
}

bool ott::ssdp::join(void)
{
    sockaddr_in self_sin;

    ssdp_sin.sin_family=self_sin.sin_family=AF_INET;
    ssdp_sin.sin_addr.s_addr=inet_addr(ssdp_group_addr);
    self_sin.sin_addr.s_addr=INADDR_ANY;
    ssdp_sin.sin_port=self_sin.sin_port=htons(ssdp_group_port);

#if defined(__FreeBSD__) || defined(__APPLE__)
    ssdp_sin.sin_len=self_sin.sin_len=sizeof(sockaddr_in);
#endif

    int sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

    if(sock==-1)
        return false;

    int reuse=1, mcast_ttl=1, mcast_loop=0, bufsize=config::ssdp_buf_size; in_addr_t ifaddr=inet_addr(config::if_addr.c_str());

    ip_mreq mcast_group;
    memset((char*)&mcast_group,0,sizeof(mcast_group));
    mcast_group.imr_multiaddr.s_addr=ssdp_sin.sin_addr.s_addr;
    mcast_group.imr_interface.s_addr=ifaddr;

    setsockopt(sock,SOL_SOCKET,SO_SNDBUF,&bufsize,sizeof(bufsize));

    setsockopt(sock,SOL_SOCKET,SO_RCVBUF,&bufsize,sizeof(bufsize));

    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));

    if(!setsockopt(sock,IPPROTO_IP,IP_MULTICAST_TTL,(const char*)&mcast_ttl,sizeof(mcast_ttl)) &&
        !setsockopt(sock,IPPROTO_IP,IP_MULTICAST_LOOP,(const char*)&mcast_loop,sizeof(mcast_loop)) &&
            !setsockopt(sock,IPPROTO_IP,IP_MULTICAST_IF,(const char*)&ifaddr,sizeof(ifaddr)) &&
                !bind(sock,(sockaddr*)&self_sin,sizeof(self_sin)) &&
                    !setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group)))
                        fd=sock;

    if(fd==-1)
        { close(sock); return false; }

    register_services(alive_list,"ssdp:alive");

    register_services(byebye_list,"ssdp:byebye");

    return true;
}

std::string ott::ssdp::trim(const std::string& s,bool tolow)
{
    std::string::size_type p1=s.find_first_not_of(' ');

    if(p1!=std::string::npos)
    {
        std::string::size_type p2=s.length();

        while(p2>0 && s[p2-1]==' ')
            --p2;

        if(tolow)
        {
            std::string ss; ss.reserve(p2-p1);

            for(std::string::size_type i=p1;i<p2;++i)
                ss+=tolower(s[i]);

            return ss;
        }else
            return s.substr(p1,p2-p1);
    }

    return std::string();
}

void ott::ssdp::doit(void)
{
    for(std::list<datagram>::const_iterator it=queue.begin();it!=queue.end();++it)
        doit(it->data,it->sin,config::get_server_date());

    queue.clear();
}

bool ott::ssdp::find_service(const std::string& s)
{
    for(int i=0;service_list[i];i++)
        if(s==service_list[i])
            return true;

    return false;
}

void ott::ssdp::send_resp(const std::string& date,const std::string& st,const std::string& usn,const sockaddr_in& sin)
{
    char buf[config::tmp_buf_size];

    int n=snprintf(buf,sizeof(buf),
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=%i\r\n"
        "DATE: %s\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s/upnp/device.xml\r\n"
        "Server: %s\r\n"
        "ST: %s\r\n"
        "USN: %s\r\n\r\n",
            config::ssdp_max_age,date.c_str(),config::www_location.c_str(),config::device_description.c_str(),st.c_str(),usn.c_str());

    if(n==-1 || n>=sizeof(buf))
        n=sizeof(buf)-1;

    sendto(fd,buf,n,0,(sockaddr*)&sin,sizeof(sin));
}

std::string ott::ssdp::parse(const std::string& s,std::map<std::string,std::string>& req)
{
    int idx=0;

    std::string type;

    for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2,idx++)
    {
        std::string ss;

        p2=s.find('\n',p1);

        if(p2!=std::string::npos)
            { ss=s.substr(p1,p2-p1); p2++; }
        else
            ss=s.substr(p1);

        if(!ss.empty() && ss[ss.length()-1]=='\r')
            ss.resize(ss.length()-1);

        if(ss.empty())
            continue;

        if(idx)
        {
            std::string::size_type p3=ss.find(':');

            if(p3!=std::string::npos)
                req[trim(ss.substr(0,p3),true)]=trim(ss.substr(p3+1),false);
        }else
        {
            std::string::size_type p3=ss.find(' ');

            if(p3!=std::string::npos)
                type=trim(ss.substr(0,p3),true);
        }
    }

    return type;
}

void ott::ssdp::doit(const std::string& s,const sockaddr_in& sin,const std::string& date)
{
    std::map<std::string,std::string> req;

    std::string type=parse(s,req);

    const std::string& man=req["man"];

    if(type!="m-search" || man!="\"ssdp:discover\"")
        return;

    const std::string& st=req["st"];

    if(st!="ssdp:all" && st!=config::uuid && !find_service(st))
        return;

    logger::print("%s: %s %s [%s]",inet_ntoa(sin.sin_addr),type.c_str(),st.c_str(),req["user-agent"].c_str());

    int mx=atoi(req["mx"].c_str());

    pid_t pid=fork();

    if(!pid)
    {
        reset_signals();

        usleep(300+rand()%500);

        if(st=="ssdp:all" || st==config::uuid)
        {
            for(int i=0;ssdp::service_list[i];i++)
            {
                if(i)
                    usleep(ssdp_send_delay_usec);

                send_resp(date,st,config::uuid+"::"+ssdp::service_list[i],sin);
            }
        }else
            send_resp(date,st,config::uuid+"::"+st,sin);

        exit(0);
    }
}

volatile int ott::ssdp::__sig_quit=0;

volatile int ott::ssdp::__sig_alrm=0;

volatile int ott::ssdp::__sig_chld=0;

void ott::ssdp::__sig_handler(int n)
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

void ott::ssdp::reset_signals(void)
    { common::reset_signals(); }

void ott::ssdp::on_alarm(void)
{
    time_t now=time(0);

    bool refresh=false;

    if(config::register_pause>0 && (!register_time || now-register_time>=config::register_pause))
        { register_time=now; refresh=true; }

    pid_t pid=fork();

    if(!pid)
    {
        reset_signals();

        send_alive();

        if(refresh)
        {
            std::string uuid;

            std::string::size_type n=config::uuid.find(':');

            if(n!=std::string::npos)
                uuid=config::uuid.substr(n+1);
            else
                uuid=config::uuid;

            // system(...);
        }

        exit(0);
    }else if(pid>0)
        alarm(config::ssdp_max_age-3);  // запас 3 сек на всякий случай
}

bool ott::ssdp::loop(void)
{
    if(!join())
        return false;

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

    on_alarm();

    while(!__sig_quit)
    {
        fd_set fdset; FD_ZERO(&fdset);

        FD_SET(fd,&fdset);

        int rc=pselect(fd+1,&fdset,NULL,NULL,NULL,&emptyset);

        if(rc==-1)
        {
            if(errno==EINTR)
            {
                if(__sig_chld)
                {
                    pid_t pid; int status;

                    while((pid=wait4(-1,&status,WNOHANG,NULL))>0);

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
            char buf[config::tmp_buf_size];

            ssize_t len;

            sockaddr_in sin; socklen_t sinlen=sizeof(sin);

            int count=0;

            while((len=recvfrom(fd,buf,sizeof(buf),MSG_DONTWAIT,(sockaddr*)&sin,&sinlen))>0)
            {
                if(++count<=config::ssdp_queue_size)
                {
                    queue.push_back(datagram());

                    datagram& s=queue.back();

                    s.data.assign(buf,len); s.sin=sin;
                }
            }

            doit();
        }
    }

    sigprocmask(SIG_SETMASK,&emptyset,NULL);

    send_bye();

    return true;
}

static void __find_handler(int) {}

bool ott::ssdp::search(const std::string& if_addr,const std::string& st,int mx,std::set<std::string>& lst)
{
    // готовимся к рассылке в мультикаст группу

    if(mx<1) mx=1; else if(mx>5) mx=5;

    int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);

    if(fd==-1)
        return false;

    int mcast_ttl=1, mcast_loop=0, bufsize=config::ssdp_buf_size; in_addr_t ifaddr=inet_addr(if_addr.c_str());

    sockaddr_in sin;
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=inet_addr(ssdp_group_addr);

    sin.sin_port=htons(ssdp_group_port);

#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sockaddr_in);
#endif

    setsockopt(fd,SOL_SOCKET,SO_SNDBUF,&bufsize,sizeof(bufsize));

    setsockopt(fd,SOL_SOCKET,SO_RCVBUF,&bufsize,sizeof(bufsize));

    if(setsockopt(fd,IPPROTO_IP,IP_MULTICAST_TTL,(const char*)&mcast_ttl,sizeof(mcast_ttl)) ||
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_LOOP,(const char*)&mcast_loop,sizeof(mcast_loop)) ||
            setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,(const char*)&ifaddr,sizeof(ifaddr)))
                { close(fd); return false; }

    char buf[config::tmp_buf_size];

    // шлем поисковый запрос

    int n=snprintf(buf,sizeof(buf),
        "M-SEARCH * HTTP/1.1\r\n"
        "HOST: %s:%i\r\n"
        "USER-AGENT: %s\r\n"
        "MX: %i\r\n"
        "ST: %s\r\n"
        "MAN: \"ssdp:discover\"\r\n\r\n",
            ssdp_group_addr,ssdp_group_port,config::device_description.c_str(),mx,st.c_str());

    if(n==-1 || n>=sizeof(buf))
        n=sizeof(buf)-1;

    sendto(fd,buf,n,0,(sockaddr*)&sin,sizeof(sin));

    // ждем результатов

    struct sigaction act,oldact;

    sigfillset(&act.sa_mask); act.sa_flags=0; act.sa_handler=__find_handler;

    sigaction(SIGALRM,&act,&oldact);

    sigset_t fullset; sigfillset(&fullset);

    sigset_t emptyset; sigemptyset(&emptyset);

    sigprocmask(SIG_BLOCK,&fullset,NULL);

    alarm(mx);

    for(;;)
    {
        fd_set fdset; FD_ZERO(&fdset); FD_SET(fd,&fdset);

        if(pselect(fd+1,&fdset,NULL,NULL,NULL,&emptyset)!=1 || !FD_ISSET(fd,&fdset))
            break;

        ssize_t n=recv(fd,buf,sizeof(buf),0);

        if(n==-1 || n==0)
            break;

//        printf("%s\n",std::string(buf,n).c_str());

        std::map<std::string,std::string> resp;

        std::string type=parse(std::string(buf,n),resp);

        const std::string& s=resp["location"];

        if(s.substr(0,7)=="http://")
            lst.insert(s);
    }

    alarm(0);

    sigprocmask(SIG_UNBLOCK,&fullset,NULL);

    sigaction(SIGALRM,&oldact,NULL);

    close(fd);

    return true;
}

/*
int main(void)
{
    std::set<std::string> lst;

//    ott::ssdp::search("192.168.90.80","urn:schemas-upnp-org:service:AVTransport:1",3,lst);
    ott::ssdp::search("192.168.1.126","ssdp:all",3,lst);

    for(std::set<std::string>::const_iterator it=lst.begin();it!=lst.end();++it)
        printf("%s\n",it->c_str());

    return 0;
}
*/
