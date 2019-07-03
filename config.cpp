/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

namespace config
{
    std::string cache_path;

    std::string www_root="./html";

    std::string if_addr;

    std::string www_location;

    std::string device_description="eXtensible UPnP agent";

    std::string model="xupnpd-live";

    std::string version="1.0";

    std::string device_name="OTTBox";

    std::string uuid;

/*
    If DLNA.ORG_OP=11, then left/right keys uses range header, and up/down uses TimeSeekRange.DLNA.ORG header
    If DLNA.ORG_OP=10, then left/right and up/down keys uses TimeSeekRange.DLNA.ORG header
    If DLNA.ORG_OP=01, then left/right keys uses range header, and up/down keys are disabled
    and if DLNA.ORG_OP=00, then all keys are disabled
*/

    std::string dlna_extras=
        "Content-Type: video/mpeg\r\n"  // video/MP2T
        "Content-Disposition: attachment; filename=\"stream.ts\"\r\n"
//        "ContentFeatures.DLNA.ORG: DLNA.ORG_PN=MPEG_TS_HD_NA;DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000\r\n"
        "ContentFeatures.DLNA.ORG: DLNA.ORG_OP=00;DLNA.ORG_CI=0;DLNA.ORG_FLAGS=01700000000000000000000000000000\r\n"
        "TransferMode.DLNA.ORG: Streaming\r\n";

    std::string user_agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/65.0.3325.146 Safari/537.36";

    std::string log;            // "x.x.x.x:40514/134";

    int update_id=0;

    std::map<std::string,std::string> env;
}

std::string config::get_server_date(void)
{
    time_t timestamp=time(NULL);

    tm* t=gmtime(&timestamp);

    char buf[128];

    int n=strftime(buf,sizeof(buf),"%a, %d %b %Y %H:%M:%S GMT",t);

    return std::string(buf,n);
}

std::string config::uuidgen(void)
{
    u_int16_t t[8];

    char buf[64];

    for(int i=0;i<sizeof(t)/sizeof(*t);i++)
        t[i]=rand()&0xffff;

    int n=sprintf(buf,"%.4x%.4x-%.4x-%.4x-%.4x-%.4x%.4x%.4x",t[0],t[1],t[2],t[3],t[4],t[5],t[6],t[7]);

    return std::string(buf,n);
}

const char* config::get_mime_type(const std::string& s)
{
    if(s=="html")       return "text/html";
    else if(s=="txt")   return "text/plain";
    else if(s=="xml")   return "text/xml; charset=\"UTF-8\"";
    else if(s=="css")   return "text/css";
    else if(s=="js")    return "application/javascript";
    else if(s=="m3u")   return "audio/x-mpegurl";               // application/x-mpegURL
    else if(s=="ico")   return "image/vnd.microsoft.icon";
    else if(s=="jpeg")  return "image/jpeg";
    else if(s=="png")   return "image/png";

    return "application/x-octet-stream";
}
