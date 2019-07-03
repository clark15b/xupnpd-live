/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "server.h"
#include "config.h"
#include <stdio.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "log.h"

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

    srand(time(NULL));

    int rc;

    if(!path.empty())
        rc=chdir(path.c_str());

    bool daemon=false;

    const char* media="";

    const char* listener="";

    bool no_announce=false;

    int opt;

    while((opt=getopt(argc,argv,"dm:l:c:n"))!=-1)
    {
        switch(opt)
        {
        case 'd':
            daemon=true;
            break;
        case 'm':
            media=optarg;
            break;
        case 'l':
            listener=optarg;
            break;
        case 'c':
            config::cache_path=optarg;
            break;
        case 'n':
            no_announce=true;
            break;
        }
    }

    if(!*media)
        media="./media";

    if(!*listener)
        listener="40000";

    if(config::cache_path.empty())
        config::cache_path="./";

    if(config::cache_path[config::cache_path.size()-1]!='/')
        config::cache_path+="/";

    static const char uuid_dat[]="xupnpd.uid";

    FILE* fp=fopen(uuid_dat,"r");

    if(fp)
    {
        char buf[256];

        if(fgets(buf,sizeof(buf),fp))
        {
            char* p=strpbrk(buf,"\r\n");

            if(p)
                *p=0;

            if(strlen(buf)==36)
                config::uuid.assign(buf,36);
        }

        fclose(fp);
    }

    if(config::uuid.empty())
    {
        config::uuid=config::uuidgen();

        FILE* fp=fopen(uuid_dat,"w");

        if(fp)
            { fprintf(fp,"%s\n",config::uuid.c_str()); fclose(fp); }
    }

    if(daemon)
    {
        pid_t pid=fork();

        if(pid==-1)
            { perror("fork"); _exit(1); }
        else if(pid)
            _exit(0);

        int fd=open("/dev/null",O_RDWR);

        if(fd!=-1)
        {
            for(int i=0;i<3;i++)
                dup2(fd,i);

            close(fd);
        }

        logger::open(config::log);
    }else
        logger::open("stderr");

    logger::ident=config::uuid.substr(28);

    config::device_name+=" "; config::device_name+=config::version;

    config::uuid=std::string("uuid:")+config::uuid;

    config::env["device_name"]=config::device_name;

    config::env["device_description"]=config::device_description;

    config::env["model"]=config::model;

    config::env["version"]=config::version;

    config::env["uuid"]=config::uuid;

    ott::server srv;

    srv.media=media;

    srv.doit(listener,!no_announce);

    logger::close();

    return 0;
}
