/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __SSDP_H
#define __SSDP_H

#include <string>
#include <list>
#include <map>
#include <set>
#include <netinet/in.h>

namespace ott
{
    struct datagram
    {
        sockaddr_in sin;

        std::string data;
    };

    class ssdp
    {
    public:
        int fd;

        std::list<datagram> queue;

    protected:
        sockaddr_in ssdp_sin;

        static const char* service_list[];

        std::list<std::string> alive_list,byebye_list;

        time_t register_time;

        void register_msg(std::list<std::string>& t,const std::string& nt,const std::string& usn,const std::string& nts);

        void register_services(std::list<std::string>& t,const std::string& nts);

        bool send_announce(const std::list<std::string>& t);

        static std::string trim(const std::string& s,bool tolow);

        static std::string parse(const std::string& s,std::map<std::string,std::string>& req);

        void doit(const std::string& s,const sockaddr_in& sin,const std::string& date);

        bool find_service(const std::string& s);

        void send_resp(const std::string& date,const std::string& st,const std::string& usn,const sockaddr_in& sin);

        static volatile int __sig_quit;

        static volatile int __sig_alrm;

        static volatile int __sig_chld;

        static void __sig_handler(int n);

        void reset_signals(void);
    public:
        ssdp(void):fd(-1),register_time(0) {}

        ~ssdp(void);

        bool join(void);

        bool send_alive(void)
            { return send_announce(alive_list); }

        bool send_bye(void)
            { return send_announce(byebye_list); }

        void doit(void);

        void on_alarm(void);

        bool loop(void);

        static bool search(const std::string& if_addr,const std::string& st,int mx,std::set<std::string>& lst);
    };
}

#endif /* __SSDP_H */
