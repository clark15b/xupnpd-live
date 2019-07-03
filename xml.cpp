/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "xml.h"
#include <ctype.h>

bool xml::parser::parse(const char* p,int len)
{
    for(;len>0;++p,--len)
    {
        int ch=*((const unsigned char*)p);

        switch(st)
        {
        case 0:
            if(ch=='<')
                st=1;
            else
                data+=ch;
            break;
        case 1:
            if(isalpha(ch))
                { tok+=ch; st=10; data.clear(); }
            else if(ch=='/')
                st=20;
            else
                st=2;
            break;
        case 2:
            if(ch=='>')
                reset();
            break;
        case 10:
            if(ch=='>')
                { stack.push_back(tok); ctrl->beg(tok,attr); reset(); }
            else if(ch=='/')
                st=30;
            else
            {
                if(!is_attr)
                {
                    if(ch==' ')
                        is_attr=true;
                    else
                        tok+=ch;
                }else
                    attr+=ch;
            }
            break;
        case 20:
            if(ch!='>')
                tok+=ch;
            else
            {
                if(stack.empty() || stack.back()!=tok)
                    return false;

                stack.pop_back();

                ctrl->end(tok,data);

                reset();
            }
            break;
        case 30:
            if(ch=='>')
                { ctrl->beg(tok,attr); ctrl->end(tok,data); reset(); }
            else if(ch=='/')
                { if(!is_attr) tok+=ch; else attr+=ch; }
            else
            {
                if(!is_attr)
                    { tok+='/'; tok+=ch; }
                else
                    { attr+='/'; attr+=ch; }
                st=10;
            }
            break;
        }
    }

    return true;
}

bool xml::parse(controller* c,const char* s,int len)
{
    parser p(c);

    if(p.begin() && p.parse(s,len) && p.end())
        return true;

    return false;
}

/*
int main(void)
{
    xml::dump c;

    char s[]=
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n"
        "<s:Envelope s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\"><s:Body><u:GetSearchCapabilities xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">xxx\nyyy</u:GetSearchCapabilities><test/></s:Body></s:Envelope>";

    if(xml::parse(&c,s,sizeof(s)-1))
        printf("OK\n");

    return 0;
}
*/
