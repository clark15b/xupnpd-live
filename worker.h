/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __WORKER_H
#define __WORKER_H

#include <stdio.h>
#include <string>
#include <map>
#include <vector>
#include "m3u8.h"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ott
{
    class worker
    {
    protected:
        static worker* self;

        int cmdfd;                                      // дескриптор для отправки статуса родительскому процессу (addref)

        int timeout;                                    // время чтения запроса в сек (после этого alarm)

        bool init;

        const m3u8::playlist& playlist;                 // ссылка на плейлист

        std::string peer_addr;                          // адрес клиента

        std::vector<std::string> req_line;              // строка HTTP запроса разбитая на составляющие ('GET', '/', 'HTTP/1.0')

        std::map<std::string,std::string> headers;      // HTTP заголовки, ключ в верхнем регистре, вместо '-' подставлено '_'

        std::string req_body;                           // тело запроса

        std::string url;                                // URL как есть

        std::string path;                               // путь к ресурсу из URL

        std::string filename;                           // название ресурса из URL

        std::string shortname;                          // название ресурса из URL без расширения

        std::string filetype;                           // тип запрашиваемого ресурса из URL

        std::map<std::string,std::string> args;         // параметры GET запроса

        bool read_http_request(void);

        bool parse_url(void);

        bool status(int code,const char* msg,bool ext=false);

        bool addref(const std::string& url);

        std::string get_next_file(const std::string& path,long& last);

        long get_file_size(int fd);

        bool sendmedia(const std::string& path);

        bool sendfile(void);

        static int lua_status(lua_State* L);

        bool dofile(void);
    public:
        worker(int _cmdfd,int _timeout,const m3u8::playlist& _p,const std::string& ip):cmdfd(_cmdfd),timeout(_timeout),
            init(true),playlist(_p),peer_addr(ip) { self=this; }

        bool doit(void);
    };
}

#endif /* __WORKER_H */
