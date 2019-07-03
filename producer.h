/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __PRODUCER_H
#define __PRODUCER_H

#include <string>
#include "m3u8.h"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace ott
{
    class producer
    {
    protected:
        std::string url;

        lua_State* L;

        const m3u8::playlist& playlist;

        std::string translate_url(const std::string& url_translator,const std::string& url);

        void doit(const std::string& path,int stream_id,const std::string& via,int retry,const std::string& proxy,int cache_ms);
    public:
        producer(const std::string& _url,const m3u8::playlist& _p):url(_url),L(NULL),playlist(_p) {}

        void doit(void);
    };
}

#endif /* __PRODUCER_H */
