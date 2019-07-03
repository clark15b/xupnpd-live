/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "defaults.h"
#include "plugin_hls.h"
#include "plugin_lua.h"
#include "hls.h"
#include <stdio.h>

namespace hls
{
    std::string translate_url(const std::string& url_translator,const std::string& url)
    {
        if(url_translator[0]=='@')
            return lua::translate_url(url_translator.c_str()+1,url);

        std::string real_url;

        char buf[512];

        int n=snprintf(buf,sizeof(buf),"./%s '%s'",url_translator.c_str(),url.c_str());

        if(n==-1 || n>=sizeof(buf))
            buf[sizeof(buf)-1]=0;

        FILE* fd=popen(buf,"r");

        if(fd)
        {
            if(fgets(buf,sizeof(buf),fd))
                real_url=buf;

            pclose(fd);
        }

        return real_url;
    }
}

void hls::doit(const std::string& path,int stream_id,const std::string& via,int retry,const std::string& proxy)
{
    if(via.empty())
        retry=0;

    time_t next_update=0;

    std::string chunklist_url;

    bool prefill=true;

    hls::chunk c;

    int fails=0;

    while(fails<defaults::hls_find_chunk_retries)
    {
        hls::timeval tv;

        if(retry>0 && tv.time()>=next_update)
            { chunklist_url.clear(); next_update=tv.time()+retry; }

        if(chunklist_url.empty())
        {
            for(int i=0;i<defaults::hls_find_list_retries;i++)
            {
                std::string url(path);

                if(!via.empty())
                    url=translate_url(via,url);

                if(!url.empty())
                    chunklist_url=hls::get_chunklist_url(url,stream_id);

                if(chunklist_url.empty())
                    usleep(defaults::hls_retry_delay*1000);
                else
                    break;
            }

            if(chunklist_url.empty())
                return;

            fprintf(stderr,"hls[%u]: %s\n",getpid(),chunklist_url.c_str());
        }

        if(prefill)
        {
            std::list<hls::chunk> chunks;

            if(hls::get_first_chunk(chunklist_url,chunks,c))
            {
                hls::chunk& cc=chunks.back();

                hls::transfer_chunk(tv,cc.url,cc.duration,1,false);

//                fprintf(stderr,"prefill %i, %ims\n",cc.seq,cc.duration);
            }

            prefill=false;
        }else
        {
            if(hls::get_next_chunk(chunklist_url,c))
            {
//                fprintf(stderr,"found %i, %ims\n",c.seq,c.duration);

                hls::transfer_chunk(tv,c.url,c.duration,1,true);

/*
                unsigned long elapsed=tv.elapsed_ms();

                if(elapsed<c.duration)
                    usleep((c.duration-elapsed)*1000);
*/

                fails=0;
            }else
            {
//                fprintf(stderr,"not found\n");

                usleep(defaults::hls_retry_delay*1000);

                fails++;
            }
        }
    }
}

void http::doit(const std::string& path,const std::string& via,const std::string& proxy)
{
    std::string url(path);

    if(!via.empty())
        url=hls::translate_url(via,url);

    if(url.empty())
        return;

    fprintf(stderr,"http[%u]: %s\n",getpid(),url.c_str());

    hls::transfer_url(url,1);
}
