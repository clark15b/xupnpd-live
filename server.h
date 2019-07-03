/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __SERVER_H
#define __SERVER_H

#include <string>
#include <map>
#include <netinet/in.h>
#include "m3u8.h"
#include "ssdp.h"
#include "curl.h"

namespace ott
{
    class stream
    {
    public:
        int nref;

        pid_t pid;

        std::string url;

        stream(void):nref(0),pid(0) {}
    };

    class server
    {
    protected:
        class ssdp ssdp;

        curl::context curl_ctx;

        std::map<std::string,stream> streams;

        std::map<pid_t,stream*> clients;

        m3u8::playlist playlist;

        static volatile int __sig_quit;

        static volatile int __sig_alrm;

        static volatile int __sig_chld;

        static void __sig_handler(int n);

        void reset_signals(void);

        std::string get_best_interface(void);

        std::string get_if_addr(const std::string& interface);

        int listen(const std::string& addr);

        void rmdir(const std::string& path);

        template<typename T> T max(T a,T b) { return a>b?a:b; }

        bool update_media(void);
    public:
        std::string media;
    public:
        server(void) {}

        bool doit(const std::string& addr,bool ssdp_reg);

        void on_alarm(void);

        void on_addref(int pid,const std::string& url);

        void on_release(int pid);
    };
}

#endif /* __SERVER_H */
