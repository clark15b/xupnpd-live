/*
 * Copyright (C) 2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "config.h"
#include "worker.h"
#include "luacompat.h"
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#if !defined(__FreeBSD__) && !defined(__APPLE__)
#include <sys/sendfile.h>
#endif

#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include "log.h"
#include "luaaux.h"

namespace ott
{
    worker* worker::self=NULL;

    char* trim(char* beg,char* end)
    {
        if(end)
        {
            while(end>beg && end[-1]==' ')
                    end--;

            *end=0;
        }

        while(*beg==' ')
            beg++;

        return beg;
    }

    void split(char* s,const char* delim,std::vector<std::string>& toks)
    {
        char* tmp;

        char* p=strtok_r(s,delim,&tmp);

        while(p)
        {
            toks.push_back(p);

            p=strtok_r(NULL,delim,&tmp);
        }
    }

    char* strupr(char* s)
    {
        for(unsigned char* p=(unsigned char*)s;*p;++p)
            *p=(*p=='-')?'_':toupper(*p);

        return s;
    }
}

bool ott::worker::read_http_request(void)
{
    if(timeout>0)
        alarm(timeout);

    char buf[config::tcp_rcv_buf_size];

    int line=0;

    req_line.reserve(3);

    while(fgets(buf,sizeof(buf),stdin))
    {
        char* p=strpbrk(buf,"\r\n");

        if(!p)
            break;

        p=trim(buf,p);

        if(!*p)
            break;

        if(!line)
            split(p," ",req_line);
        else
        {
            char* p2=strchr(p,':');

            if(p2)
                headers[strupr(trim(p,p2))]=trim(p2+1,NULL);
        }

        line++;
    }

    std::map<std::string,std::string>::const_iterator it=headers.find("CONTENT_LENGTH");

    if(it!=headers.end())
    {
        int len=atoi(it->second.c_str());

        if(len<0 || len>config::content_length_max)
            return false;

        while(len>0)
        {
            size_t n=fread(buf,1,len>sizeof(buf)?sizeof(buf):len,stdin);

            if(!n)
                break;

            req_body.append(buf,n);

            len-=n;
        }

        if(len>0)
            return false;
    }

    if(timeout>0)
        alarm(0);

    if(req_line.size()==3 && req_line[2].length()==8 && !strncmp(req_line[2].c_str(),"HTTP/1.",7) && (req_line[2][7]=='0' || req_line[2][7]=='1'))
        return true;


    return false;
}

void ott::worker::fix_url(void)
{
    std::string& url=req_line[1];

    if(!url.empty())
    {
        const char* s=url.c_str();

        while(s[1]=='/')
            s++;

        if(s>url.c_str())
            url=url.substr(s-url.c_str());
    }
}

bool ott::worker::parse_url(void)
{
    fix_url();

    url=req_line[1];

    if(url[0]!='/' || url.find("../")!=std::string::npos)
        return false;

    std::string::size_type n=url.find('?');

    std::string s;

    if(n!=std::string::npos)
        { s=url.substr(n+1); url=url.substr(0,n); }

    if(url=="/")
        url="/index.html";

    n=url.find_last_of('/');

    if(n!=std::string::npos)
        { path=url.substr(0,++n); filename=url.substr(n); }

    n=filename.find_last_of('.');

    if(n!=std::string::npos)
        { shortname=filename.substr(0,n); filetype=filename.substr(n+1); }

    for(std::string::size_type p1=0,p2;p1!=std::string::npos;p1=p2)
    {
        std::string ss;

        p2=s.find('&',p1);

        if(p2==std::string::npos)
            ss=s.substr(p1);
        else
            { ss=s.substr(p1,p2-p1); p2++; }

        if(!ss.empty())
        {
            n=ss.find('=');

            if(n!=std::string::npos)
                args[ss.substr(0,n)]=ss.substr(n+1);
            else
                args[ss];
        }
    }

    return true;
}

bool ott::worker::status(int code,const char* msg,bool ext)
{
    fprintf(stdout,"HTTP/1.1 %i %s\r\n",code,msg);
    fprintf(stdout,"Server: %s\r\n",config::device_description.c_str());
    fprintf(stdout,"Date: %s\r\n",config::get_server_date().c_str());
    fprintf(stdout,"Connection: close\r\n");
    fprintf(stdout,"EXT:\r\n");

    if(!ext)
        { fprintf(stdout,"\r\n"); fflush(stdout); }

    return true;
}

bool ott::worker::addref(const std::string& url)
{
    char buf[config::tmp_buf_size];

    int n=snprintf(buf,sizeof(buf),"%i %s",(int)getpid(),url.c_str());

    if(n==-1 || n>=sizeof(buf))
        return false;

    n++;

    if(send(cmdfd,buf,n,0)!=n)
        return false;

    return true;
}

bool ott::worker::doit(void)
{
    if(!read_http_request() || !parse_url())
        return status(400,"Bad Request");

    srand(time(NULL));

    config::env["peer_addr"]=peer_addr;

    logger::print("%s: %s %s [%s]",peer_addr.c_str(),req_line[0].c_str(),req_line[1].c_str(),headers["USER_AGENT"].c_str());

    // стриминг
    if(path=="/" && filetype=="ts")
    {
        const m3u8::node* stream=playlist.find(shortname);

        if(!stream)
            return status(404,"Not Found");

        status(200,"OK",true);

        fwrite(config::dlna_extras.c_str(),1,config::dlna_extras.length(),stdout);

        fprintf(stdout,"\r\n");

        fflush(stdout);

        if(req_line[0]!="HEAD")
        {
            mkdir((config::cache_path+shortname).c_str(),S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);

            if(addref(shortname))
            {
                bool rc=sendmedia(config::cache_path+shortname+"/");

                logger::print("%s: connection %s [%s]",peer_addr.c_str(),rc?"closed by peer":"timeout",headers["USER_AGENT"].c_str());
            }
        }

        return true;
    }

    if(filetype=="lua")
        dofile();           // исполнение скрипта
    else
        sendfile();     // отправка статики

    return true;
}

std::string ott::worker::get_next_file(const std::string& path,long& last)
{
    std::map<int,std::string> files;

    DIR* h=opendir(path.c_str());

    if(h)
    {
        dirent* de;

        while((de=readdir(h)))
        {
            if(de->d_name[0]!='.')
            {
                char* endptr=NULL;

                long num=strtol(de->d_name,&endptr,10);

                if(!strcmp(endptr,".ts"))
                    files[num]=de->d_name;
            }
        }

        closedir(h);
    }

    std::map<int,std::string>::iterator it=files.upper_bound(last);

    if(it==files.end())
        return std::string();

    if(!init)
    {
        if(it->first>last+config::max_seq/2)
            return std::string();
    }else
        init=false;

    last=it->first;

    if(last>=config::max_seq)
        last=0;

//    logger::print("found %s",it->second.c_str());

    return path+it->second;
}

long ott::worker::get_file_size(int fd)
{
    size_t len=lseek(fd,0,SEEK_END);

    lseek(fd,0,SEEK_SET);

    return len;
}

#if defined(__FreeBSD__) || defined(__APPLE__)
ssize_t sendfile(int out_fd,int in_fd,off_t*,size_t count)
{
    ssize_t total=0;

    while(total<count)
    {
        char buf[2048];

        size_t left=count-total;

        ssize_t n=read(in_fd,buf,left>sizeof(buf)?sizeof(buf):left);

        if(!n || n==-1)
            break;

        ssize_t m=write(out_fd,buf,n);

        if(m==-1)
            m=0;

        total+=m;

        if(m<n)
            { lseek(in_fd,-(n-m),SEEK_CUR); break; }
    }

    return total;
}
#endif

bool ott::worker::sendmedia(const std::string& path)
{
    int infd_fl=fcntl(fileno(stdin),F_GETFL); int outfd_fl=fcntl(fileno(stdout),F_GETFL);

    fcntl(fileno(stdin),F_SETFL,infd_fl|O_NONBLOCK); fcntl(fileno(stdout),F_SETFL,outfd_fl|O_NONBLOCK);

    long seq=0;

    int fd=-1;

    size_t len=0;

    bool retval=true;

    for(;;)
    {
        int maxfd=fileno(stdin);

        fd_set rfdset; FD_ZERO(&rfdset); FD_SET(fileno(stdin),&rfdset);

        fd_set wfdset; FD_ZERO(&wfdset);

        if(fd==-1)
        {
            std::string s=get_next_file(path,seq);

            if(!s.empty())
            {
                fd=open(s.c_str(),O_RDONLY);

                if(fd!=-1)
                {
                    len=get_file_size(fd);

                    if(!len || len==-1)
                        { close(fd); fd=-1; }
                    else
                        fcntl(fd,F_SETFL,O_NONBLOCK);
                }
            }
        }

        timeval tv; tv.tv_usec=0;

        if(fd!=-1)
        {
            FD_SET(fileno(stdout),&wfdset);

            if(fileno(stdout)>fileno(stdin))
                maxfd=fileno(stdout);

            tv.tv_sec=config::tcp_snd_timeout;
        }else
            tv.tv_sec=1;

        int rc=select(maxfd+1,&rfdset,&wfdset,NULL,&tv);

        if(rc==-1)
            break;
        else if(rc==0)
        {
            if(fd!=-1)
                { retval=false; break; }

            continue;
        }

        if(FD_ISSET(fileno(stdin),&rfdset))
        {
            char buf[config::tmp_buf_size];

            int n=read(fileno(stdin),buf,sizeof(buf));

            if(n==-1 || n==0)           // соединение закрыто
                break;
        }

        if(fd!=-1 && FD_ISSET(fileno(stdout),&wfdset))
        {
            ssize_t n=::sendfile(fileno(stdout),fd,NULL,len);

            if(n==-1)
            {
                if(errno==EAGAIN || errno==EWOULDBLOCK)
                    continue;
                else
                    break;
            }

            len-=n;

            if(!n || len<1)
                { close(fd); fd=-1; }
        }
    }

    if(fd!=-1)
        close(fd);

    fcntl(fileno(stdin),F_SETFL,infd_fl); fcntl(fileno(stdout),F_SETFL,outfd_fl);

    return retval;
}


bool ott::worker::sendfile(void)
{
    enum { max_name_len=64 };

    std::string s=config::www_root+url;

    bool tmpl=false;

    int fd=open((s+".tmpl").c_str(),O_RDONLY);

    if(fd!=-1)
        tmpl=true;
    else
        fd=open(s.c_str(),O_RDONLY);

    if(fd==-1)
        return status(404,"Not Found");

    struct stat sb;

    if(fstat(fd,&sb) || (sb.st_mode&S_IFMT)!=S_IFREG)
        { close(fd); return status(403,"Forbidden"); }

    size_t len=get_file_size(fd);

    status(200,"OK",true);

    fprintf(stdout,"Content-Type: %s\r\n",config::get_mime_type(filetype));

    if(!tmpl)
    {
        fprintf(stdout,"Content-Length: %lu\r\n\r\n",(unsigned long)len);

        fflush(stdout);

        while(len>0)
        {
            ssize_t n=::sendfile(fileno(stdout),fd,NULL,len);

            if(n==0 || n==-1)
                break;

            len-=n;
        }
    }else
    {
        std::string ss;

        char buf[1024];

        int st=0;

        std::string name; name.reserve(max_name_len);

        while(len>0)
        {
            ssize_t n=read(fd,buf,sizeof(buf)>len?len:sizeof(buf));

            if(n==0 || n==-1)
                break;

            len-=n;

            for(int i=0;i<n;i++)
            {
                int ch=((unsigned char*)buf)[i];

                switch(st)
                {
                case 0:
                    if(ch!='$')
                        ss+=ch;
                    else
                        st=1;
                    break;
                case 1:
                    if(ch!='{')
                    {
                        ss+='$';

                        if(ch!='$')
                            { ss+=ch; st=0; }
                    }else
                        st=2;
                    break;
                case 2:
                    if(ch!='}')
                    {
                        if(name.length()<max_name_len)
                            name+=ch;
                    }else
                    {
                        std::map<std::string,std::string>::const_iterator it=config::env.find(name);

                        if(it!=config::env.end())
                            ss+=it->second;

                        name.clear(); st=0;
                    }

                    break;
                }
            }
        }

        fprintf(stdout,"Content-Length: %lu\r\n\r\n",(unsigned long)ss.length());

        size_t l=0;

        while(l<ss.length())
        {
            size_t n=fwrite(ss.c_str()+l,1,ss.length()-l,stdout);

            if(!n)
                break;

            l+=n;
        }
    }

    fflush(stdout);

    close(fd);

    return true;
}

int ott::worker::lua_status(lua_State* L)
{
    int code=0; const char* s=NULL;

    if(lua_gettop(L)>0)
        code=lua_tonumber(L,1);

    if(lua_gettop(L)>1)
        s=lua_tostring(L,2);

    self->status(code?code:200,s?s:"OK",true);

    if(lua_gettop(L)>2 && lua_type(L,3)==LUA_TTABLE)
    {
        lua_pushnil(L);

        while(lua_next(L,3))
        {
            const char* ss=lua_tostring(L,-1);

            if(ss)
                fprintf(stdout,"%s\r\n",ss);

            lua_pop(L,1);
        }
    }

    fprintf(stdout,"\r\n"); fflush(stdout);

    return 0;
}

bool ott::worker::dofile(void)
{
    chdir((config::www_root+path).c_str());

    struct stat sb;

    if(stat(filename.c_str(),&sb) || (sb.st_mode&S_IFMT)!=S_IFREG)
        return status(403,"Forbidden");

    lua_State* L=lua_open();

    if(!L)
        return status(500,"Internal Server error");

    lua::init(L);

    lua_pushlstring(L,req_line[0].c_str(),req_line[0].length()); lua_setglobal(L,"method");

    lua::lua_push_map(L,"args",args); args.clear();

    lua::lua_push_map(L,"headers",headers); headers.clear();

    lua_pushlstring(L,req_body.c_str(),req_body.length()); lua_setglobal(L,"content"); req_body.clear();

    lua_register(L,"status",lua_status);

    if(luaL_loadfile(L,filename.c_str()) || lua_pcall(L,0,0,0))
        { logger::print("%s",lua_tostring(L,-1)); lua_pop(L,1); status(500,"Internal Server Error",false); }

    lua_close(L);

    return true;
}
