#ifndef __LOG_H
#define __LOG_H

#include <string>

namespace logger
{
    extern std::string ident;

    void open(const std::string& s);

    void print(const char* fmt,...);

    void close(void);
}

#endif /* __LOG_H */
