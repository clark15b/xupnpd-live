/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "producer.h"
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "hls.h"
#include "config.h"
#include "log.h"
#include "luaaux.h"

std::string ott::producer::translate_url(const std::string& url_translator,const std::string& url)
{
    std::string real_url;

    lua_getglobal(L,url_translator[0]=='@'?url_translator.c_str()+1:url_translator.c_str());

    if(lua_type(L,-1)==LUA_TFUNCTION)
    {
        lua_pushlstring(L,url.c_str(),url.length());

        if(!lua_pcall(L,1,1,0))
        {
            size_t n=0;

            const char* p=lua_tolstring(L,-1,&n);

            if(p && n>0)
                real_url.assign(p,n);

            lua_pop(L,1);
        }else
            { logger::print("%s",lua_tostring(L,-1)); lua_pop(L,1); }
    }else
        lua_pop(L,1);

    return real_url;
}

void ott::producer::doit(void)
{
    const m3u8::node* stream=playlist.find(url);

    if(!stream)
        return;

    if(!(L=lua_open()))
        return;

    lua::init(L);

    if(luaL_loadfile(L,"xupnpd-live.lua") || lua_pcall(L,0,0,0))
        { logger::print("%s",lua_tostring(L,-1)); lua_pop(L,1); lua_close(L); return; }

    chdir((config::cache_path+url).c_str());

    doit(stream->get_url(),stream->get_stream(),stream->get_via(),stream->get_retry(),stream->get_proxy(),stream->get_cache_ms());

    lua_close(L);
}

void ott::producer::doit(const std::string& path,int stream_id,const std::string& via,int retry,const std::string& proxy,int cache_ms)
{
    if(via.empty())
        retry=0;

    if(cache_ms<1)
        cache_ms=config::cache_ms;

    time_t next_update=0;

    std::string chunklist_url;

    std::list<hls::chunk> chunks;

    int last=0;

    int seq=0;

    unsigned long cached_ms=0;

    std::list<hls::chunk> cache_files;

    pid_t pid=getpid();

    for(;;)
    {
        hls::timeval tv;

        if(retry>0 && tv.time()>=next_update)
            { chunklist_url.clear(); next_update=tv.time()+retry; }

        if(chunklist_url.empty())
        {
            std::string url(path);

            if(!via.empty())
                url=translate_url(via,url);

            if(!url.empty())
                chunklist_url=hls::get_chunklist_url(url,stream_id);

            if(chunklist_url.empty())
                { usleep(config::hls_refresh_pause*1000); continue; }

            logger::print("hls[%u]: %s",pid,chunklist_url.c_str());
        }

        if(chunks.empty())                              // нововведение!!! раньше опрашивал перед каждым новым фрагментом
            hls::get_chunks(chunklist_url,chunks,last);

        if(chunks.empty())
            { usleep(config::hls_refresh_pause*1000); continue; }

        last=chunks.back().seq;

        hls::chunk& c=chunks.front();

        if(config::debug)
            logger::print("hls[%u]: found fragment %i, duration=%i, url=%s",pid,c.seq,c.duration,c.url.c_str());

        while(cached_ms+c.duration>cache_ms && !cache_files.empty())
        {
            hls::chunk& cc=cache_files.front();

            unlink(cc.url.c_str());

            cached_ms-=cc.duration;

            cache_files.pop_front();
        }

        if(++seq>config::max_seq)
            seq=1;

        char filename[128]; sprintf(filename,"%.4i.ts",seq);

        std::string tmp(filename); tmp+='~';

        int fd=open(tmp.c_str(),O_CREAT|O_TRUNC|O_WRONLY,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);

        if(fd!=-1)
        {
            bool rc=hls::transfer_chunk(c.url,fd);

            close(fd);

            if(rc)
            {
                rename(tmp.c_str(),filename);

                if(config::debug)
                    logger::print("hls[%u]: fragment %i is ready, path=%s",pid,c.seq,filename);
            }else
                unlink(tmp.c_str());
        }else
            { usleep(config::hls_refresh_pause*1000); continue; }

        unsigned long duration=c.duration;

        cached_ms+=duration;

        cache_files.push_back(hls::chunk());
        cache_files.back().duration=duration;
        cache_files.back().url=filename;

        unsigned long elapsed=tv.elapsed_ms();

        chunks.pop_front();

        if(chunks.empty() && elapsed<duration)
            usleep((duration-elapsed)*1000);
    }
}

