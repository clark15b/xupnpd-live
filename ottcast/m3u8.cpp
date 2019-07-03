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

namespace m3u8
{
    int get_cache_size(const std::string& s)
    {
        const char* p=s.c_str();

        char* endptr=NULL;

        int n=strtol(p,&endptr,10);

        if(*endptr=='k' || *endptr=='K')
            n*=1000;
        else if(*endptr=='m' || *endptr=='M')
            n*=1000000;

        return n;
    }

    bool get_boolean(const std::string& s)
    {
        if(s=="1" || s=="true")
            return true;

        return false;
    }
}

void m3u8::playlist::parse_attr(constructor* c,const char* s)
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
                { c->set(name,buf); name.clear(); buf.clear(); st=0; }
            else
                buf+=ch;
            break;
        case 3:
            if(ch=='\"')
                { c->set(name,buf); name.clear(); buf.clear(); st=0; }
            else
                buf+=ch;
            break;
        }
    }

    if(st==2)
        c->set(name,buf);
}

void m3u8::channel::set(const std::string& name,std::string& value)
{
    if(name=="name")
        channel::name.swap(value);
    else if(name=="url")
        url.swap(value);
    else if(name=="cache")
        cache=get_cache_size(value);
    else if(name=="stream")
        stream=atoi(value.c_str());
    else if(name=="loop")
        loop=get_boolean(value);
}

void m3u8::folder::set(const std::string& name,std::string& value)
{
    if(name=="name")
        folder::name.swap(value);
    else if(name=="source")
        source.swap(value);
    else if(name=="interface")
        interface.swap(value);
    else if(name=="via")
        via.swap(value);
    else if(name=="proxy")
        proxy.swap(value);
    else if(name=="retry")
        retry=atoi(value.c_str());
    else if(name=="cache")
        cache=get_cache_size(value);
}

int m3u8::playlist::parse(const std::string& filename)
{
    FILE* fp=fopen(filename.c_str(),"r");

    if(!fp)
        return false;

//    fprintf(stderr,"%s\n",filename.c_str());

    folder& fldr=folders[folders.size()];

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

    channel chnl;

    char buf[512];

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

                        parse_attr(&chnl,p);
                    }
                }else if(!strncmp(p+1,"EXTM3U",6))
                    parse_attr(&fldr,p+7);
            }else
            {
                channel& c=fldr.channels[fldr.channels.size()];

                c.parent=&fldr;

                c.name.swap(chnl.name);

                c.url=p;

                c.cache=chnl.cache;

                if(c.cache<1)
                    c.cache=fldr.cache;

                c.stream=chnl.stream;

                c.loop=chnl.loop;

                chnl.clear();
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

    return count;
}

m3u8::channel* m3u8::playlist::find(const std::string& u)
{
    std::string::size_type n=u.find('c');

    if(n==std::string::npos)
        return NULL;

    int fid=atoi(u.substr(0,n).c_str());

    int cid=atoi(u.substr(n+1).c_str());

    if(fid<0 || cid<0)
        return NULL;

    std::map<int,folder>::iterator fit=folders.find(fid);

    if(fit==folders.end())
        return NULL;

    std::map<int,channel>::iterator cit=fit->second.channels.find(cid);

    if(cit==fit->second.channels.end())
        return NULL;

    return &(cit->second);
}

/*
int main(void)
{
    m3u8::playlist p;

    p.scan("media/");

    for(std::map<int,m3u8::folder>::const_iterator i=p.folders.begin();i!=p.folders.end();++i)
    {
        const m3u8::folder& f=i->second;

        printf("name='%s' source='%s', interface='%s', via='%s', retry=%i\n",f.name.c_str(),f.source.c_str(),f.interface.c_str(),
            f.via.c_str(),f.retry);

        for(std::map<int,m3u8::channel>::const_iterator j=f.channels.begin();j!=f.channels.end();++j)
        {
            const m3u8::channel& c=j->second;

            printf("   name='%s', url='%s', cache=%i\n",c.name.c_str(),c.url.c_str(),c.cache);
        }
    }


    return 0;
}
*/
