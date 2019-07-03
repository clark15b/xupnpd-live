/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "defaults.h"
#include <string.h>
#include "ev_epoll.h"
#include "m3u8.h"
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

namespace cfg
{
    bool parse(const std::string& cfg,std::map<std::string,std::string>& opts);

    char* trim(char* beg,char* end,bool* add=NULL)
    {
        char* p=end?end:beg+strlen(beg);

        if(add && p>beg && p[-1]=='\\')
            { *add=true; p--;}

        while(p>beg && (p[-1]==' ' || p[-1]=='\t'))
            p--;

        *p=0;

        for(p=beg;*p==' ' || *p=='\t';p++);

        return p;
    }
}

bool cfg::parse(const std::string& cfg,std::map<std::string,std::string>& opts)
{
    FILE* fp=fopen(cfg.c_str(),"r");

    if(!fp)
        return false;

    char buf[512];

    std::string* last=NULL;

    while(fgets(buf,sizeof(buf),fp))
    {
        char* p=strpbrk(buf,"#\r\n");

        bool add=false;

        p=trim(buf,p,&add);

        if(*p)
        {
            if(last)
            {
                if(!last->empty())
                    last->append("\r\n");

                last->append(p);
            }else
            {
                char* p2=strchr(p,'=');

                if(p2)
                {
                    p=trim(p,p2);

                    p2=trim(p2+1,NULL);

                    if(*p)
                        { last=&opts[p]; last->assign(p2); }
                }
            }

            if(!add)
                last=NULL;
        }
    }

    fclose(fp);

    return true;
}

static void mkpls(const std::string& path)
{
    m3u8::playlist p;

    p.scan(path);

    int cid=0; int oid=0;

    for(std::map<int,m3u8::folder>::const_iterator i=p.folders.begin();i!=p.folders.end();++i,++cid)
    {
        oid=0;

        const m3u8::folder f=i->second;

        char name[256];

        sprintf(name,"lv%.6i.m3u",cid);

        FILE* fp=fopen(name,"w");

        if(fp)
        {
            fprintf(fp,"#EXTM3U\n");

            fprintf(fp,"#EXT-X-M3U: type=ts name=\"%s\"\n\n",f.name.c_str());

            for(std::map<int,m3u8::channel>::const_iterator j=f.channels.begin();j!=f.channels.end();++j,++oid)
            {
                const m3u8::channel c=j->second;

                fprintf(fp,"#EXTINF:0,%s\n",c.name.c_str());

                fprintf(fp,"/%ic%i.ts\n\n",cid,oid);
            }

            fclose(fp);
        }
    }
}

int main(int argc,char** argv)
{
    std::string self,path;

    {
        const char* p=strrchr(argv[0],'/');

        if(p)
            { path.assign(argv[0],p-argv[0]); self=p+1; }
        else
            self=argv[0];
    }

    int rc;

    if(!path.empty())
        rc=chdir(path.c_str());

    std::string conf;

    bool daemon=false;

    int startup_delay=0;

    int opt;

    while((opt=getopt(argc,argv,"dc:t:p:"))!=-1)
    {
        switch(opt)
        {
        case 'd':
            daemon=true;
            break;
        case 'c':
            conf=optarg;
            break;
        case 't':
            startup_delay=atoi(optarg);
            break;
        case 'p':
            mkpls(optarg);
            return 0;
        }
    }

    if(conf.empty())
        conf="xupnpd-live.cfg";

    if(daemon)
    {
        pid_t pid=fork();

        if(pid==-1)
            { perror("fork"); _exit(1); }
        else if(pid)
            _exit(0);

        int fd=open("/dev/null",O_RDWR);

        if(fd!=1)
        {
            for(int i=0;i<3;i++)
                dup2(fd,i);

            close(fd);
        }
    }

    if(startup_delay>0)
        sleep(startup_delay);

    ev_epoll::engine::initialize();

    ev_epoll::engine engine;

    ev::controller controller(&engine);

    std::map<std::string,std::string> opts;

    cfg::parse(conf,opts);

    controller.scan(opts["media"]);

    controller.listen(opts["listen"],defaults::backlog_size);

    ev::timeout=atoi(opts["timeout"].c_str());

    if(ev::timeout<1)
        ev::timeout=defaults::timeout;

    ev::interface=opts["interface"];

    ev::dlna_extras=opts["dlna_extras"];

    ev::broadcast_cmd=opts["broadcast_cmd"];

    ev::cache_size=m3u8::get_cache_size(opts["cache"]);

    if(ev::cache_size<1)
        ev::cache_size=defaults::cache_size;

    engine.loop(&controller);

    return 0;
}
