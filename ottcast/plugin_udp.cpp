/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_udp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>

namespace udp
{
    bool nake_rtp_packet(const unsigned char*& p,int& len)
    {
        if(len<12)
            return false;

        if( (*p & 0xc0)>>6 !=2 )
            return false;

        bool padding=(*p & 0x20)?true:false;

        bool extras=(*p & 0x10)?true:false;

        int csrcl=((*p & 0x0f) + 3)*4;

        p+=csrcl; len-=csrcl;

        if(len<0)
            return false;

        if(extras)
        {
            if(len<4)
                return false;

            int ehl=p[2]; ehl<<=8; ehl+=p[3]; ehl++; ehl*=4;

            p+=ehl; len-=ehl;

            if(len<0)
                return false;
        }

        if(padding)
        {
            if(len<1)
                return false;

            len-=p[len-1];

            if(len<0)
                return false;
        }

        return true;
    }

    enum { proto_udp=1, proto_rtp=2 };

    in_addr_t get_if_addr(const std::string& ifname);

    int openurl(const std::string& url,int& proto,const std::string& ifaddr);
}

in_addr_t udp::get_if_addr(const std::string& ifname)
{
    if(ifname.empty())
        return INADDR_ANY;

    in_addr_t addr=inet_addr(ifname.c_str());

    if(addr!=INADDR_NONE)
        return addr;

    ifreq ifr;

    int n=snprintf(ifr.ifr_name,IFNAMSIZ,"%s",ifname.c_str());

    if(n==-1 || n>=IFNAMSIZ)
        return INADDR_ANY;

    int s=socket(AF_INET,SOCK_DGRAM,0);

    if(s==-1)
        return INADDR_ANY;

    if(ioctl(s,SIOCGIFADDR,&ifr)==-1 || ifr.ifr_addr.sa_family!=AF_INET)
        { close(s); return INADDR_ANY; }

    close(s);

    return ((sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
}

int udp::openurl(const std::string& url,int& proto,const std::string& ifaddr)
{
    proto=0;

    bool multicast=false;

    const char* p=url.c_str();

    if(!strncmp(p,"udp://",6))
        proto=proto_udp;
    else if(!strncmp(p,"rtp://",6))
        proto=proto_rtp;
    else
        return -1;

    p+=6;

    if(*p=='@')
        { multicast=true; p++; }

    const char* p2=strchr(p,':');

    if(!p2)
        return -1;

    in_addr_t addr=inet_addr(std::string(p,p2-p).c_str());

    sockaddr_in sin;
    sin.sin_family=AF_INET;
    sin.sin_addr.s_addr=addr;
    sin.sin_port=htons(atoi(p2+1));
#if defined(__FreeBSD__) || defined(__APPLE__)
    sin.sin_len=sizeof(sin);
#endif /* __FreeBSD__ */

    int sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);

    if(sock!=-1)
    {
        int reuse=1; setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&reuse,sizeof(reuse));

        if(!bind(sock,(sockaddr*)&sin,sizeof(sin)))
        {
            bool is_ok=true;

            if(multicast)
            {
                ip_mreq mcast_group;

                memset((char*)&mcast_group,0,sizeof(mcast_group));
                mcast_group.imr_multiaddr.s_addr=addr;
                mcast_group.imr_interface.s_addr=get_if_addr(ifaddr);

                if(setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group)))
                    is_ok=false;
            }

            if(is_ok)
            {
                //fcntl(sock,F_SETFL,O_NONBLOCK);

                return sock;
            }
        }

        close(sock);
    }

    return -1;
}

void udp::doit(const std::string& url,const std::string& multicast_interface)
{
    fprintf(stderr,"udp[%u]: %s (%s)\n",getpid(),url.c_str(),multicast_interface.c_str());

    int proto=0;

    int sock=openurl(url,proto,multicast_interface);

    if(sock==-1)
        return;

    char buf[4096];

    int n;

    while((n=recv(sock,buf,sizeof(buf),0))>0)
    {
        const unsigned char* ptr=(const unsigned char*)buf;

        int len=n;

        if(proto==proto_rtp && !nake_rtp_packet(ptr,len))
            break;

        int l=0;

        while(l<len)
        {
            int m=write(1,ptr+l,len-l);

            if(m==-1 || !m)
                break;

            l+=m;
        }
    }

    //setsockopt(incoming_socket,IPPROTO_IP,IP_DROP_MEMBERSHIP,(const char*)&mcast_group,sizeof(mcast_group));

    close(sock);
}
