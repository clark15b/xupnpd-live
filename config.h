/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <string>
#include <map>

namespace config
{
    enum
    {
        backlog                 = 10,           // длина очереди полуоткрытых соединений
        timeout                 = 15,           // время ожидания HTTP запроса в сек
        tmp_buf_size            = 1024,         // максимальная длина временных буферов
        ssdp_buf_size           = 16384,        // длина буфера отправки и приема SSDP пакетов
        ssdp_queue_size         = 32,           // максимальное количество входящих SSDP пакетов в очереди
        ssdp_max_age            = 900,          // время жизни SSDP анонса в сек
        tcp_lookup_timeout      = 3,            // время на резрешение имен
        tcp_connect_timeout     = 3,            // время на установление соединения
        tcp_timeout             = 10,           // максимальное время ожидания при чтении и записи в сек
        tcp_snd_timeout         = 30,           // максимальное время ожидания при отправке потока клиенту в сек
        tcp_rcv_buf_size        = 4096,         // длина буфера чтения
        tcp_reconnect_attempts  = 4,            // количество попыток установления TCP соединения
        tcp_reconnect_pause     = 250,          // задержка между попытками установления соединения в мс
        header_length_max       = 512,          // максимальная длина заголовка
        headers_max             = 32,           // максимальное количество заголовков
        content_length_max      = 512000,       // максимальная длина тела HTTP ответа (для req_simple)
        hls_refresh_pause       = 500,          // задержка между попытками в мс
        hls_chunks_max          = 32,           // максимальное количество загружаемых элементов плейлиста (последние)
        cache_ms                = 30000,        // время кэша в мс по умолчанию
        max_seq                 = 9999,         // максимальный номер файла в кэше, после этого сброс на 1 ("%.4i.ts" в producer.cpp)
        register_pause          = 0,
        debug                   = 0
    };

    extern std::string cache_path;

    extern std::string www_root;

    extern std::string if_addr;

    extern std::string www_location;

    extern std::string device_description;

    extern std::string model;

    extern std::string version;

    extern std::string device_name;

    extern std::string uuid;

    extern std::string dlna_extras;

    extern std::string user_agent;

    extern std::string log;

    extern int update_id;

    extern std::map<std::string,std::string> env;

    std::string get_server_date(void);

    std::string uuidgen(void);

    const char* get_mime_type(const std::string& s);
}

#endif /* __CONFIG_H */

