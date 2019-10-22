/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "m3u8.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <set>
#include <list>
#include "config.h"

namespace m3u8
{
    playlist* playlist::self=NULL;
}

void m3u8::node::parse_attr(const char* s)
{
    int st=0;

    std::string name,buf;

    for(;*s;s++)
    {
        int ch=*((const unsigned char*)s);

        switch(st)
        {
        case 0:
            if(ch!=' ')
                { name+=ch; st=1; }
            break;
        case 1:
            if(ch==' ')
                { name.clear(); st=0; }
            else if(ch=='=')
                st=2;
            else
                name+=ch;
            break;
        case 2:
            if(ch=='\"')
                st=3;
            else if(ch==' ')
                { set(name,buf); name.clear(); buf.clear(); st=0; }
            else
                buf+=ch;
            break;
        case 3:
            if(ch=='\"')
                { set(name,buf); name.clear(); buf.clear(); st=0; }
            else
                buf+=ch;
            break;
        }
    }

    if(st==2)
        set(name,buf);
}

void m3u8::node::set(const std::string& name,std::string& value)
{
    if(name=="tvg-name")
    {
        if(!value.empty())
            node::name.swap(value);
    }else if(name=="stream")
        stream=atoi(value.c_str());
    else if(name=="via")
        via.swap(value);
    else if(name=="proxy")
        proxy.swap(value);
    else if(name=="refresh")
        retry=atoi(value.c_str());
    else if(name=="cache_ms")
        cache_ms=atoi(value.c_str());
    else
        other[name]=value;
}

int m3u8::playlist::parse(const std::string& filename)
{
    {
        std::string::size_type n=filename.find_last_of('.');

        if(n==std::string::npos || strncasecmp(filename.substr(n+1).c_str(),"m3u",3))
            return 0;
    }

    FILE* fp=fopen(filename.c_str(),"r");

    if(!fp)
        return false;

    int fid=folders.size();

    node& fldr=folders[fid];

    fldr.id=fid;

    {
        std::string s;

        std::string::size_type n=filename.find_last_of('/');

        s=(n==std::string::npos)?filename:filename.substr(n+1);

        n=s.find_last_of('.');

        if(n!=std::string::npos)
            fldr.name=s.substr(0,n);
        else
            fldr.name.swap(s);
    }

    node chnl;

    char buf[1024];

    int id=0;

    while(fgets(buf,sizeof(buf),fp))
    {
        char* p=strpbrk(buf,"\r\n");

        if(!p)
            p=buf+strlen(buf);

        while(p>buf && (p[-1]==' ' || p[-1]=='\t'))
            p--;

        *p=0;

        for(p=buf;*p==' ' || *p=='\t';p++);

        if(*p)
        {
            if(*p=='#')
            {
                if(!strncmp(p+1,"EXTINF:",7))
                {
                    p+=8;

                    char* p2=strchr(p,',');

                    if(p2)
                    {
                        *p2=0; p2++;

                        while(*p2==' ' || *p2=='\t')
                            p2++;

                        chnl.name=p2;

                        chnl.parse_attr(p);
                    }
                }else if(!strncmp(p+1,"EXTM3U",6))
                    fldr.parse_attr(p+7);
            }else
            {
                node& c=fldr.channels[fldr.channels.size()];

                c.swap(chnl);

                c.parent=&fldr;

                c.url=p;

                c.id=id++;
            }
        }
    }

    fclose(fp);

    return fldr.channels.size();
}

int m3u8::playlist::scan(const std::string& dirname)
{
    int count=0;

    std::set<std::string> files;

    std::string path(dirname);

    if(path.empty() || path[path.length()-1]!='/')
        path+='/';

    DIR* h=opendir(dirname.c_str());

    if(h)
    {
        dirent* de;

        while((de=readdir(h)))
        {
            if(de->d_name[0]!='.')
                files.insert(de->d_name);
        }

        closedir(h);
    }


    for(std::set<std::string>::const_iterator it=files.begin();it!=files.end();++it)
        count+=parse(path+*it);

    config::update_id++;

    return count;
}

const m3u8::node* m3u8::playlist::find(const std::string& u) const
{
    std::string::size_type n=u.find('c');

    if(n==std::string::npos)
        return NULL;

    int fid=atoi(u.substr(0,n).c_str());

    int cid=atoi(u.substr(n+1).c_str());

    if(fid<0 || cid<0)
        return NULL;

    std::map<int,node>::const_iterator fit=folders.find(fid);

    if(fit==folders.end())
        return NULL;

    std::map<int,node>::const_iterator cit=fit->second.channels.find(cid);

    if(cit==fit->second.channels.end())
        return NULL;

    return &(cit->second);
}

/*
int main(void)
{
    m3u8::playlist p;

    p.scan("../xupnpd-live/media/");

    for(std::map<int,m3u8::node>::const_iterator i=p.folders.begin();i!=p.folders.end();++i)
    {
        const m3u8::node& f=i->second;

        printf("%s\n",f.get_name().c_str());

        for(std::map<int,m3u8::node>::const_iterator j=f.channels.begin();j!=f.channels.end();++j)
        {
            const m3u8::node& c=j->second;

            printf("   name=%s: %s [stream=%i, via=%s, proxy=%s, retry=%i]\n",
                c.get_name().c_str(),c.get_url().c_str(),c.get_stream(),c.get_via().c_str(),c.get_proxy().c_str(),c.get_retry());
        }
    }

    return 0;
}
*/
