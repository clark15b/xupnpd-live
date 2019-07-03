/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __EV_BASE_H
#define __EV_BASE_H

#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <set>

namespace ev
{
    // допустимые типы сокетов
    namespace types
    {
        enum
        {
            listener    = 1,
            client      = 2,
            source      = 3
        };
    };

    // базовый класс для всех типов сокетов
    class socket
    {
    public:
        int type;                               // тип

        int fd;                                 // файловый дескриптор

        bool is_monitored;                      // входит в состав epoll/kqueue

        bool is_pollin;                         // готов к чтению

        bool is_pollout;                        // готов к записи

        bool is_pollhup;                        // соединение закрыто или ошибка
    private:
        socket(void) {}                         // нельзя создать просто так, только с инициализацией
    public:
        socket(int t):type(t),fd(-1),is_monitored(false),is_pollin(false),is_pollout(false),is_pollhup(false) {}

        ~socket(void);
    };

    class listener : public socket
    {
    public:
        listener(void):socket(types::listener) {}
    };

    class client : public socket
    {
    public:
        int st;

        std::string buf;

        int snd_offset;

        client* tm_prev,*tm_next;

        time_t tm_expiry;                       // время истечения ожидания запроса

        int rd_offset;                          // позиция чтения в текущем циклическом буфере

        class source* src;                      // ссылка на источник

    public:
        client(void):socket(types::client),st(0),snd_offset(0),tm_prev(NULL),tm_next(NULL),tm_expiry(0),rd_offset(0),src(NULL) {}
    };

    class source : public socket
    {
    public:
        char* buf;                              // указатель на кольцевой буфер

        int size;                               // длина буфера кратная 188

        int wr_offset;                          // позиция записи

        bool rotate;                            // буфер провернулся

        std::string url;                        // наименование источника

        std::set<client*> clients;              // список всех клиентов, асоциированных с этим источником

        pid_t pid;                              // идентификатор дочернего процесса
    public:
        source(void):socket(types::source),buf(NULL),size(0),wr_offset(0),rotate(false),pid(0) {}

        ~source(void);

        bool set_buf_size(int len);             // выделить память под буфер
    };

    class scheduler
    {
    protected:
        client* beg,*end;
    public:
        scheduler(void):beg(NULL),end(NULL) {}

        client* insert(client* c,time_t expiry);

        client* remove(client* c);

        client* fetch(time_t now);

        time_t get_wake_delay(time_t now);
    };
}

#endif /* __EV_BASE_H */
