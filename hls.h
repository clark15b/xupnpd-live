/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __HLS_H
#define __HLS_H

#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <list>

namespace hls
{
    class chunk
    {
    public:
        int seq;                                // номер фрагмента или стрима в мастерлисте

        int duration;                           // время фрагмента в мс

        std::string inf;                        // заполняется только для мастер листов (EXT-X-STREAM-INF)

        std::string url;

    public:
        chunk(void):seq(0),duration(0) {}
    };

    class timeval
    {
    protected:
        ::timeval tv0;
    public:
        timeval(void) { now(); }

        void now(void) { gettimeofday(&tv0,NULL); }

        unsigned long elapsed_ms(void)
        {
            ::timeval tv;

            gettimeofday(&tv,NULL);

            return (tv.tv_sec-tv0.tv_sec)*1000 + (tv.tv_usec-tv0.tv_usec)/1000;
        }

        time_t time(void)
            { return tv0.tv_sec; }
    };

    std::string get_chunklist_url(const std::string& url,int stream_id);

    bool get_next_chunk(const std::string& url,chunk& c);

    bool get_chunks(const std::string& url,std::list<chunk>& chunks,int last);

    bool transfer_chunk(const std::string& url,int fd);
}

#endif /* __HLS_H */
