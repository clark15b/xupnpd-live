/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "plugin_broadcast.h"
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <set>

void broadcast::doit(const std::string& path,const std::string& broadcast_cmd,bool loop)
{
    std::set<std::string> playlist;

    if(!path.empty() && path[path.length()-1]=='/')
    {
        DIR* h=opendir(path.c_str());

        if(h)
        {
            dirent* de;

            while((de=readdir(h)))
            {
                if(de->d_name[0]!='.')
                    playlist.insert(path+de->d_name);
            }

            closedir(h);
        }
    }else
        playlist.insert(path);

    do
    {
        for(std::set<std::string>::const_iterator it=playlist.begin();it!=playlist.end();++it)
        {
            struct stat st;

            if(!lstat(it->c_str(),&st) && (st.st_mode & S_IFREG))
            {
                char buf[512];

                if(snprintf(buf,sizeof(buf),broadcast_cmd.c_str(),it->c_str())<sizeof(buf))
                {
                    fprintf(stderr,"broadcast[%u]: %s\n",getpid(),buf);

                    int rc=system(buf);
                }
            }
        }
    }while(loop);
}
