/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __EV_EPOLL_H
#define __EV_EPOLL_H

#include "ev_controller.h"

namespace ev_epoll
{
    class engine : public ev::engine
    {
    protected:
        int efd;

        static volatile int __sig_quit;

        static volatile int __sig_alrm;

        static volatile int __sig_chld;

        static void __sig_handler(int n);

        bool ctl(ev::socket* s,int op);
    public:
        engine(void);

        ~engine(void);

        virtual bool set(ev::socket* s);

        virtual bool del(ev::socket* s);

        void loop(ev::controller* ctrl);

        static void initialize(void);   // глобальная инициализация
    };
}

#endif /* __EV_EPOLL_H */
