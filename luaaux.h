#ifndef __LUAAUX_H
#define __LUAAUX_H

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <string>
#include <map>

namespace lua
{
    void lua_push_map(lua_State* L,const std::string& name,const std::map<std::string,std::string>& m);

    void init(lua_State* L);
}


#endif /* __LUAAUX_H */
