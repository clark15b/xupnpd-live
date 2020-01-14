#include "xml.h"

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

static int lua_soap_parse(lua_State* L)
{
    class soap : public xml::controller
    {
    protected:
        lua_State* L;
    public:
        soap(lua_State* _L):L(_L) { lua_newtable(L); }

        virtual void beg(const std::string& tag,const std::string& attr)
        {
            std::string::size_type n=tag.find(':');

            if(n!=std::string::npos)
                lua_pushstring(L,tag.substr(n+1).c_str());
            else
                lua_pushlstring(L,tag.c_str(),tag.length());

            lua_newtable(L);
        }

        virtual void end(const std::string& tag,const std::string& data)
        {
            if(!data.empty())
            {
                lua_pushstring(L,"value");
                lua_pushlstring(L,data.c_str(),data.length());
                lua_rawset(L,-3);
            }

            lua_rawset(L,-3);
        }
    };

    size_t l=0;

    const char* s=lua_tolstring(L,1,&l);

    if(!s || l<1)
        return 0;

    int top=lua_gettop(L);

    soap ctrl(L);

    if(!xml::parse(&ctrl,s,l))
        { lua_pop(L,lua_gettop(L)-top); return 0; }

    return 1;
}

static int lua_xml_parse(lua_State* L)
{
    class soap : public xml::controller
    {
    protected:
        lua_State* L;

        std::list<int> cn;
    public:
        soap(lua_State* _L):L(_L),cn(0) { lua_newtable(L); cn.push_back(0); }

        virtual void beg(const std::string& tag,const std::string& attr)
        {
            lua_pushinteger(L,++cn.back());
            lua_newtable(L);

            lua_pushstring(L,"name"); lua_pushlstring(L,tag.c_str(),tag.length()); lua_rawset(L,-3);
            lua_pushstring(L,"attr"); lua_pushlstring(L,attr.c_str(),attr.length()); lua_rawset(L,-3);

            cn.push_back(0);
        }

        virtual void end(const std::string& tag,const std::string& data)
        {
            if(!data.empty())
                { lua_pushstring(L,"value"); lua_pushlstring(L,data.c_str(),data.length()); lua_rawset(L,-3); }

            lua_rawset(L,-3);

            cn.pop_back();
        }
    };

    size_t l=0;

    const char* s=lua_tolstring(L,1,&l);

    if(!s || l<1)
        return 0;

    int top=lua_gettop(L);

    soap ctrl(L);

    if(!xml::parse(&ctrl,s,l))
        { lua_pop(L,lua_gettop(L)-top); return 0; }

    return 1;
}

static int lua_soap_escape_xml(lua_State* L)
{
    const unsigned char* s=(const unsigned char*)lua_tostring(L,1);

    if(!s)
        return 0;

    luaL_Buffer buf;

    luaL_buffinit(L,&buf);

    for(;*s;s++)
    {
        switch(*s)
        {
        case '<':  luaL_addlstring(&buf,"&lt;",4);      break;
        case '>':  luaL_addlstring(&buf,"&gt;",4);      break;
        case '&':  luaL_addlstring(&buf,"&amp;",5);     break;
        case '\"': luaL_addlstring(&buf,"&quot;",6);    break;
        case '\'': luaL_addlstring(&buf,"&apos;",6);    break;
        default:   luaL_addchar(&buf,*s);               break;
        }
    }

    luaL_pushresult(&buf);

    return 1;
}

extern "C" int luaopen_luasoap(lua_State* L)
{
    lua_register(L,"soap_decode",lua_soap_parse);

    lua_register(L,"xml_decode",lua_xml_parse);

    lua_register(L,"soap_escape_xml",lua_soap_escape_xml);

    return 0;
}
