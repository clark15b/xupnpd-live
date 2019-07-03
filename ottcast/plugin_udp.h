/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __PLUGIN_UDP_H
#define __PLUGIN_UDP_H

#include <string>

namespace udp
{
    void doit(const std::string& url,const std::string& multicast_interface);
}

#endif /* __PLUGIN_UDP_H */
