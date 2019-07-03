/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __M3U8_H
#define __M3U8_H

#include <map>
#include <string>

namespace m3u8
{
    class node
    {
    public:
        node* parent;

        int id;                         // порядковый номер

        std::map<std::string,std::string> other;        // остальные атрибуты
    protected:
        std::string name;               // название плейлиста

        std::string url;                // URL

        int stream;                     // номер стрима в HLS при наличие нескольких вариантов качества

        std::string via;                // имя обработчика для трансляции урлов

        std::string proxy;              // адрес прокси сервера

        int retry;                      // как часто производить повторную трансляцию урла (сек)

        int cache_ms;                   // размер кэша в мс

        void set(const std::string& name,std::string& value);

        void swap_int(int& a,int& b)
            { int n=a; a=b; b=n; }

    public:
        std::map<int,node> channels;    // каналы

    public:
        node(void):parent(NULL),id(0),stream(0),retry(0),cache_ms(0) {}

        void parse_attr(const char* s);

        void swap(node& c)
            { name.swap(c.name); url.swap(c.url); swap_int(stream,c.stream); via.swap(c.via);
                proxy.swap(c.proxy); swap_int(retry,c.retry); swap_int(cache_ms,c.cache_ms); other.swap(c.other); }

        void clear(void)
            { name.clear(); url.clear(); stream=0; via.clear(); proxy.clear(); retry=0; cache_ms=0; channels.clear(); }

        const std::string& get_name(void) const
            { return name; }

        const std::string& get_url(void) const
            { return url; }

        int get_stream(void) const
            { return (stream<1 && parent)?parent->stream:stream; }

        const std::string& get_via(void) const
            { return (via.empty() && parent)?parent->via:via; }

        const std::string& get_proxy(void) const
            { return (proxy.empty() && parent)?parent->proxy:proxy; }

        int get_retry(void) const
            { return (retry<1 && parent)?parent->retry:retry; }

        int get_cache_ms(void) const
            { return (cache_ms<1 && parent)?parent->cache_ms:cache_ms; }

        friend class playlist;
    };

    class playlist
    {
    protected:
        static playlist* self;
    public:
        std::map<int,node> folders;

    public:
        playlist(void) { self=this; }

        int parse(const std::string& filename);

        int scan(const std::string& dirname);

        const node* find(const std::string& u) const;

        static playlist& get(void) { return *self; }
    };
}

#endif /* __M3U8_H */
