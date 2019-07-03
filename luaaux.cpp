/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "luaaux.h"
#include "config.h"
#include "ssdp.h"
#include "curl.h"
#include "m3u8.h"
#include "log.h"

namespace lua
{
    int lua_playlist(lua_State* L);
    int lua_get_channel_by_id(lua_State* L);
    int lua_push_channel(lua_State* L,const m3u8::node* channel);
    int lua_uuidgen(lua_State* L);
    int lua_get_mime_type(lua_State* L);
    int lua_trace(lua_State* L);
    int lua_ssdp_search(lua_State* L);
    int lua_fetch(lua_State* L);
}

void lua::lua_push_map(lua_State* L,const std::string& name,const std::map<std::string,std::string>& m)
{
    lua_newtable(L);

    for(std::map<std::string,std::string>::const_iterator it=m.begin();it!=m.end();++it)
    {
        lua_pushlstring(L,it->first.c_str(),it->first.length());

        lua_pushlstring(L,it->second.c_str(),it->second.length());

        lua_rawset(L,-3);
    }

    lua_setglobal(L,name.c_str());
}

int lua::lua_playlist(lua_State* L)
{
    lua_newtable(L);

    for(std::map<int,m3u8::node>::const_iterator it=m3u8::playlist::get().folders.begin();it!=m3u8::playlist::get().folders.end();++it)
    {
        lua_pushnumber(L,it->second.id+1);

        lua_newtable(L);

        lua_pushstring(L,"name"); lua_pushstring(L,it->second.get_name().c_str()); lua_rawset(L,-3);

        lua_pushstring(L,"channels");

        lua_newtable(L);

        const std::map<int,m3u8::node>& channels=it->second.channels;

        for(std::map<int,m3u8::node>::const_iterator cit=channels.begin();cit!=channels.end();++cit)
            { lua_pushnumber(L,cit->second.id+1); lua_push_channel(L,&cit->second); lua_rawset(L,-3); }

        lua_rawset(L,-3);

        lua_rawset(L,-3);
    }

    return 1;
}

int lua::lua_get_channel_by_id(lua_State* L)
{
    const char* s=lua_tostring(L,1);

    if(!s)
        return 0;

    const m3u8::node* p=m3u8::playlist::get().find(s);

    if(!p)
        return 0;

    return lua_push_channel(L,p);
}

int lua::lua_push_channel(lua_State* L,const m3u8::node* channel)
{
    lua_newtable(L);

    for(std::map<std::string,std::string>::const_iterator it=channel->other.begin();it!=channel->other.end();++it)
        { lua_pushstring(L,it->first.c_str()); lua_pushstring(L,it->second.c_str()); lua_rawset(L,-3); }

    lua_pushstring(L,"name"); lua_pushstring(L,channel->get_name().c_str()); lua_rawset(L,-3);

    {
        lua_pushstring(L,"url");

        char buf[512];

        int n=snprintf(buf,sizeof(buf),"http://%s/%ic%i.ts",config::www_location.c_str(),channel->parent->id,channel->id);

        if(n==-1 || n>=sizeof(buf))
            n=sizeof(buf)-1;

        lua_pushlstring(L,buf,n);

        lua_rawset(L,-3);
    }

    lua_pushstring(L,"real_url"); lua_pushstring(L,channel->get_url().c_str()); lua_rawset(L,-3);

    return 1;
}

int lua::lua_uuidgen(lua_State* L)
{
    lua_pushstring(L,config::uuidgen().c_str());

    return 1;
}

int lua::lua_get_mime_type(lua_State* L)
{
    const char* s=lua_tostring(L,1);

    if(!s)
        return 0;

    lua_pushstring(L,config::get_mime_type(s));

    return 1;
}

int lua::lua_trace(lua_State* L)
{
    const char* s=lua_tostring(L,1);

    if(s)
        logger::print("%s",s);

    return 0;
}

int lua::lua_ssdp_search(lua_State* L)
{
    lua_newtable(L);

    const char* s=lua_tostring(L,1);

    if(s)
    {
        int mx=0;

        if(lua_gettop(L)>1)
            mx=lua_tonumber(L,2);

        std::set<std::string> lst;

        ott::ssdp::search(config::if_addr,s,mx,lst);

        int n=0;

        for(std::set<std::string>::const_iterator it=lst.begin();it!=lst.end();++it)
            { lua_pushnumber(L,++n); lua_pushstring(L,it->c_str()); lua_rawset(L,-3); }
    }

    return 1;
}

int lua::lua_fetch(lua_State* L)
{
    const char* url=lua_tostring(L,1);

    if(!url)
        return 0;

    std::string post_data;

    std::list<std::string> hdrs;

    {
        const char* data=NULL; size_t data_len=0;

        if(lua_gettop(L)>1)
            data=lua_tolstring(L,2,&data_len);

        if(data && data_len>0)
            post_data.assign(data,data_len);
    }

    if(lua_gettop(L)>2 && lua_type(L,3)==LUA_TTABLE)
    {
        lua_pushnil(L);

        while(lua_next(L,3))
        {
            const char* s=lua_tostring(L,-1);

            if(s)
                hdrs.push_back(s);

            lua_pop(L,1);
        }
    }

    curl::simple_req req(curl::context::get());

    if(req.open(url) && req.perform(post_data,hdrs,true) && req.ret_code==200)
        { lua_pushlstring(L,req.content.c_str(),req.content.length()); return 1; }

    return 0;
}

extern "C" int luaopen_luasoap(lua_State* L);

extern "C" int luaopen_luajson(lua_State* L);

void lua::init(lua_State* L)
{
    using namespace lua;

    luaL_openlibs(L);

    lua_push_map(L,"env",config::env);

    lua_register(L,"playlist",lua_playlist);

    lua_register(L,"get_channel_by_id",lua_get_channel_by_id);

    lua_register(L,"uuidgen",lua_uuidgen);

    lua_register(L,"get_mime_type",lua_get_mime_type);

    lua_register(L,"trace",lua_trace);

    lua_pushnumber(L,config::debug); lua_setglobal(L,"debug");

    lua_pushnumber(L,config::update_id); lua_setglobal(L,"update_id");

    lua_register(L,"ssdp_search",lua_ssdp_search);

    lua_register(L,"http_get",lua_fetch);

    luaopen_luasoap(L);

    luaopen_luajson(L);
}
