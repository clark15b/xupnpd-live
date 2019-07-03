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
    class constructor
    {
    public:
        virtual void set(const std::string& name,std::string& value) {}
    };

    class channel : public constructor
    {
    public:
        class folder* parent;

        std::string name;               // название канала
        std::string url;                // URL
        int cache;                      // длина циклического буфера
        int stream;                     // номер стрима в HLS при наличие нескольких вариантов качества
        bool loop;                      // повторять
    public:
        channel(void):parent(NULL),cache(0),stream(0),loop(false) {}

        virtual void set(const std::string& name,std::string& value);

        void clear(void)
            { name.clear(); url.clear(); cache=0; stream=0; loop=false; }
    };

    class folder : public constructor
    {
    public:
        std::string name;               // название плейлиста
        std::string source;             // тип источника - http,hls,udp
        std::string interface;          // интерфейс мультикаста (для udp)
        std::string via;                // имя обработчика для трансляции урлов
        std::string proxy;              // адрес прокси сервера
        int retry;                      // как часто производить повторную трансляцию урла (сек)
        int cache;                      // длина циклического буфера по умолчанию (байт)

        std::map<int,channel> channels; // каналы
    public:
        folder(void):retry(0),cache(0) {}

        virtual void set(const std::string& name,std::string& value);

        void clear(void)
            { name.clear(); source.clear(); interface.clear(); via.clear(); retry=0; cache=0; channels.clear(); }
    };

    class playlist
    {
    public:
        std::map<int,folder> folders;

    protected:
        void parse_attr(constructor* c,const char* s);

    public:
        int parse(const std::string& filename);

        int scan(const std::string& dirname);

        channel* find(const std::string& u);
    };

    int get_cache_size(const std::string& s);

    bool get_boolean(const std::string& s);
}

#endif /* __M3U8_H */
