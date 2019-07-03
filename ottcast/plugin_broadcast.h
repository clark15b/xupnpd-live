/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __PLUGIN_BROADCAST_H
#define __PLUGIN_BROADCAST_H

#include <string>

namespace broadcast
{
    void doit(const std::string& path,const std::string& broadcast_cmd,bool loop);
}

#endif /* __PLUGIN_BROADCAST_H */

