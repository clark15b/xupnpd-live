/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __LUA_H
#define __LUA_H

#include <string>

namespace lua
{
    std::string translate_url(const char* url_translator,const std::string& url);
}

#endif /* __LUA_H */
