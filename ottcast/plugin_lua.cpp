/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_lua.h"
#include "curl.h"

#ifndef NO_LUA

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace lua
{
    class req : public curl::req
    {
    public:
        luaL_Buffer* buf;
    public:
        req(luaL_Buffer* p,const std::string& s=std::string()):curl::req(s),buf(p) {}

        virtual void on_data(const char* p,int len)
            { luaL_addlstring(buf,p,len); }
    };

    void reglibs(lua_State* L);

    int lua_fetch(lua_State* L);

    int lua_trace(lua_State* L);
}

int lua::lua_fetch(lua_State* L)
{
    if(lua_gettop(L)<1)
        return 0;

    const char* url=lua_tostring(L,1);

    if(!url)
        return 0;

    luaL_Buffer buf;

    luaL_buffinit(L,&buf);

    req req(&buf);

    if(req.open(url))
        req.perform();

    luaL_pushresult(&buf);

    return 1;
}

int lua::lua_trace(lua_State* L)
{
    std::string ss;

    int count=lua_gettop(L);

    lua_getglobal(L,"tostring");

    for(int i=1;i<=count;i++)
    {
        lua_pushvalue(L,-1);

        lua_pushvalue(L,i);

        lua_call(L,1,1);

        size_t l=0;

        const char* s=lua_tolstring(L,-1,&l);

        if(s && l>0)
        {
            ss.append(s,l);

            if(i<count)
                ss+=' ';
        }

        lua_pop(L,1);
    }

    fprintf(stderr,"%s\n",ss.c_str());

    return 0;
}

void lua::reglibs(lua_State* L)
{
    luaL_openlibs(L);

    lua_register(L,"get",lua_fetch);

    lua_register(L,"print",lua_trace);
}

std::string lua::translate_url(const char* url_translator,const std::string& url)
{
    std::string real_url;

    lua_State* L=lua_open();

    if(L)
    {
        reglibs(L);

        if(!luaL_loadfile(L,"xupnpd-live.lua") && !lua_pcall(L,0,0,0))
        {
            lua_getglobal(L,url_translator);

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
                }
            }else
                lua_pop(L,1);
        }

        lua_close(L);
    }

    return real_url;
}

#else

std::string lua::translate_url(const char* url_translator,const std::string& url)
{
    return url;
}

#endif /* NO_LUA */

/*
int main(void)
{
    printf("%s\n",lua::translate_url("kineskop","http://kineskop.tv/?page=watch&ch=250").c_str());

    return 0;
}
*/
