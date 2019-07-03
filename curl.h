/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __CURL_H
#define __CURL_H

#include "config.h"
#include <string>
#include <map>
#include <list>
#include <sys/types.h>
#include <netinet/in.h>

#ifdef WITH_SSL
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#endif /* WITH_SSL */

namespace curl
{
    class connection
    {
    protected:
        int fd;

        u_int64_t id;

        char buf[config::tcp_rcv_buf_size];

        int size;

        int offset;

        bool https;

        ssize_t direct_recv(int fd,char* buf,size_t len);

        ssize_t direct_send(int fd,const char* buf,size_t len);

#ifdef WITH_SSL
        mbedtls_ssl_context ssl;

        mbedtls_net_context server_fd;

        bool ssl_connect(void);

        bool ssl_disconnect(void);

        ssize_t ssl_recv(int fd,char* buf,size_t len);

        ssize_t ssl_send(int fd,const char* buf,size_t len);

        ssize_t recv(int fd,char* buf,size_t len)
            { return https?ssl_recv(fd,buf,len):direct_recv(fd,buf,len); }

        ssize_t send(int fd,const char* buf,size_t len)
            { return https?ssl_send(fd,buf,len):direct_send(fd,buf,len); }
#else
        ssize_t recv(int fd,char* buf,size_t len)
            { return direct_recv(fd,buf,len); }

        ssize_t send(int fd,const char* buf,size_t len)
            { return direct_send(fd,buf,len); }
#endif /* WITH_SSL */
    public:
        connection(void);

        ~connection(void);

        bool send(const std::string& s);

        int recv(char** p,int len);

        bool recvln(std::string& s);

        friend class context;
    };

    class context
    {
    protected:
        static context* self;

#ifdef WITH_SSL
        mbedtls_ssl_config conf;

        mbedtls_entropy_context entropy;

        mbedtls_ctr_drbg_context ctr_drbg;
#endif /* WITH_SSL */

        std::map<std::string,in_addr_t> hosts;

        std::map<u_int64_t,connection> connections;
    public:
        context(void);

        ~context(void);

        bool lookup(const std::string& s,sockaddr_in* sin,bool https);

        connection* connect(const sockaddr_in* sin,bool https);

        connection* open(const std::string& s,bool https)
            { sockaddr_in sin; return lookup(s,&sin,https)?connect(&sin,https):NULL; }

        void close(const connection* c)
            { connections.erase(c->id); }

        void clear(void)
            { hosts.clear(); connections.clear(); }

        static context& get(void) { return *self; }

        friend class connection;
    };

    class req
    {
    protected:
        context& ctx;
    public:
        // запрос
        bool https;                                     // https://

        std::string host_name;                          // имя сервера

        std::string path;                               // путь ресурсу

        std::string url;                                // запрашиваемый ресурс

        // ответ
        int ret_code;                                   // код ответа

        bool keep_alive;                                // признак долгоживущего соединения

        long content_length;                            // длина тела ответа

        std::map<std::string,std::string> headers;      // заголовки из ответа

    protected:
        bool perform_real(connection* c,const std::string& post_data,const std::list<std::string>& hdrs);

    public:
        req(context& c,const std::string& s=std::string()):ctx(c),https(false),ret_code(0),keep_alive(false),content_length(-1) { open(s); }

        bool open(const std::string& s);

        bool perform(const std::string& post_data=std::string(),const std::list<std::string>& hdrs=std::list<std::string>(),
            bool one_shot=false);

        virtual void on_clear(void) {}

        virtual void on_data(const char* p,int len) {}
    };

    class simple_req : public req
    {
    public:
        std::string content;                            // тело ответа
    public:
        simple_req(context& c,const std::string& s=std::string()):req(c,s) {}

        virtual void on_clear(void) { content.clear(); }

        virtual void on_data(const char* p,int len);
    };

    bool connected(int fd);

    std::string trim(const std::string& s,bool toupr);
}

#endif /* __CURL_H */
