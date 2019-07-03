/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#ifndef __XML_H
#define __XML_H

#include <stdio.h>
#include <string>
#include <list>

// No <![CDATA[  ]]>

namespace xml
{
    class controller
    {
    public:
        virtual void beg(const std::string& tag,const std::string& attr) {}

        virtual void end(const std::string& tag,const std::string& data) {}
    };

    class dump : public controller
    {
    public:
        virtual void beg(const std::string& tag,const std::string& attr)
            { printf("<%s %s>\n",tag.c_str(),attr.c_str()); }

        virtual void end(const std::string& tag,const std::string& data)
        {
            if(!data.empty())
                printf("%s\n",data.c_str());

            printf("</%s>\n",tag.c_str());
        }
    };

    class parser
    {
    protected:
        controller* ctrl;

        int st;

        std::list<std::string> stack;

        bool is_attr;

        std::string tok;

        std::string attr;

        std::string data;

        void reset(void)
            { st=0; is_attr=false; tok.clear(); attr.clear(); data.clear(); }

    public:
        parser(controller* c):ctrl(c) {}

        bool begin(void)
            { reset(); return true; }

        bool end(void)
        {
            if(st || !stack.empty())
                return false;

            return true;
        }

        bool parse(const char* p,int len);
    };

    bool parse(controller* c,const char* s,int len);
}

#endif /* __XML_H */
