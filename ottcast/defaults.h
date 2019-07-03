/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __DEFAULTS_H
#define __DEFAULTS_H

namespace defaults
{
    enum
    {
        // ядро
        backlog_size            = 50,           // максимальная длина очереди полуоткрытых соединений
        max_clients             = 0,            // максимальное количество клиентов (0 - без ограничений)
        max_req_len             = 512,          // максимальная длина строки HTTP запроса
        timeout                 = 15,           // максимальное время ожидания запроса в секундах
        ts_packet_size          = 188,          // длина ts пакета (для правильного выравнивания в циклическом буфере)
        cache_size              = 512000,       // длина циклического буфера по умолчанию

        // curl
        tcp_connect_retries     = 6,            // количество попыток установления TCP соединения
        tcp_connect_retry_delay = 500,          // задержка между попытками установления соединения в мс
        tcp_timeout             = 45,           // максимальное время ожидания при чтении в сек
        tcp_buf_size            = 4096,         // длина буфера чтения

        // hls
        hls_find_list_retries   = 6,            // максимальное количество попыток получения ссылки на список фрагментов
        hls_find_chunk_retries  = 60,           // максимальное количество попыток получения следующий фрагмент до окончания трансляции
        hls_retry_delay         = 500,          // задержка между попытками в мс
        hls_max_items_num       = 32            // максимальное количество загружаемых элементов плейлиста (последние)
    };

    inline int ts_align_left(int n)
        { return n - (n % ts_packet_size); }

    inline int ts_align_right(int n)
    {
        int tail=n % ts_packet_size;

        if(tail>0)
            n+=ts_packet_size - tail;

        return n;
    }
}

#endif /* __DEFAULTS_H */
