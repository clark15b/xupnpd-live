/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __PLUGIN_HLS_H
#define __PLUGIN_HLS_H

#include <string>

namespace hls
{
    void doit(const std::string& path,int stream_id,const std::string& via,int retry,const std::string& proxy);
}

namespace http
{
    void doit(const std::string& path,const std::string& via,const std::string& proxy);
}

#endif /* __PLUGIN_HLS_H */
