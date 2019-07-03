/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __EV_CONTROLLER_H
#define __EV_CONTRILLER_H

#include "ev_base.h"
#include "m3u8.h"
#include <string>
#include <map>
#include <list>
#include <set>

namespace ev
{
    extern int timeout;

    extern std::string interface;

    extern std::string dlna_extras;

    extern std::string broadcast_cmd;

    extern int cache_size;

    // базовый класс для энджина
    class engine
    {
    public:
        virtual bool set(socket* s) { return false; }

        virtual bool del(socket* s) { return false; }
    };

    class controller
    {
    protected:
        engine* eng;

        m3u8::playlist content;                 // плейлисты с настройками

        std::map<int,listener> listeners;       // таблица со всеми дескрипторами для входящих TCP соединений

        std::map<int,client> clients;           // таблица со всеми клиентами

        std::map<int,source> sources;           // таблица со всеми источниками

        std::map<std::string,source*> urls;     // таблица всех активных урлов

        std::list<client*> trash_clnt;          // временный список клиентов на удаление, обрабатывается в flush

        std::set<source*> trash_src;            // временный список источников на удаление, обрабатывается в flush

        scheduler timers;                       // отсортированные в порядке возрастания таймеры

        std::string get_url(const std::string& buf,bool& is_head);

        source* spawn(const std::string& url);

        void attach(client* c,source* s);

        bool join(client* c,const std::string& url);

        void flush_buffer(client* c);

        static void source_main(const m3u8::channel& chnl);
    public:
        controller(engine* e):eng(e) {}

        void on_chld(pid_t pid);

        void on_alrm(void);

        void on_event(socket* s);

        void flush(void);                       // чистка trash

        bool listen(const std::string& addr,int backlog);

        int scan(const std::string& path)
            { return content.scan(path); }

        m3u8::playlist& playlist(void)
            { return content; }
    };
}

#endif /* __EV_CONTROLLER_H */
