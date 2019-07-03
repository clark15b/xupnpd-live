/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __CURL_H
#define __CURL_H

#include <string>

// TODO: HTTPS (mbedtls)

namespace curl
{
    class req
    {
    public:
        std::string host_name;                          // имя сервера

        std::string path;                               // путь ресурсу

        std::string url;                                // запрашиваемый ресурс

        int ret_code;

        int content_length;

        std::string location;                           // заполняется в случае редиректа

        bool perform_real(void);
    public:
        req(const std::string& s=std::string()):ret_code(0),content_length(0) { open(s); }

        bool open(const std::string& s);

        bool perform(void);

        virtual void on_data(const char* p,int len) {}
    };
}

#endif /* __CURL_H */



