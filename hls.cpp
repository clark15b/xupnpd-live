/*
 * Copyright (C) 2015-2018 Anton Burdinuk
 * clark15b@gmail.com
 * http://xupnpd.org
 */

#include "config.h"
#include "hls.h"
#include "curl.h"
#include <stdlib.h>

namespace hls
{
    class parser
    {
    protected:
        int st;

        int what;

        int seq;

        std::string tmp;

        std::string stream_inf;

        std::string duration;

        std::string url;

        bool is_master;

        void push(void);
    public:
        std::list<chunk> items;

        int count;

        bool master(void) const { return is_master; }

        parser(void):st(0),what(0),seq(0),is_master(false),count(0) {}

        void begin(void) { st=0; what=0; seq=0; is_master=false; count=0; }

        void parse(const char* p,int len);

        void end(void) { push(); }
    };

    class req_pls : public curl::req
    {
    protected:
        hls::parser parser;
    public:
        req_pls(const std::string& s=std::string()):curl::req(curl::context::get(),s) {}

        ~req_pls(void) { parser.end(); }

        bool master(void) { return parser.master(); }

        std::list<chunk>& items(void) { return parser.items; }

        int size(void) { return parser.count; }

        virtual void on_clear(void) { parser.begin(); }

        virtual void on_data(const char* p,int len) { parser.parse(p,len); }

        bool get_chunklist_url(const std::string& url,int stream_id,std::string& s);

        std::string get_absolute_url(const std::string& s);
    };

    class req_chunk : public curl::req
    {
    public:
        int fd;

        int bytes_sent;
    public:
        req_chunk(int _fd,const std::string& s=std::string()):curl::req(curl::context::get(),s),fd(_fd),bytes_sent(0) {}

        virtual void on_clear(void) { lseek(fd,0,SEEK_SET); }

        virtual void on_data(const char* p,int len)
        {
            int l=0;

            while(l<len)
            {
                int n=write(fd,p+l,len-l);

                if(n==0 || n==-1)
                    break;

                l+=n;

                bytes_sent+=n;
            }

//            fprintf(stderr,"%i/%i\n",bytes_sent,content_length);
        }
    };
}

void hls::parser::push(void)
{
    if(!url.empty())
    {
        if(!stream_inf.empty())
        {
            is_master=true;

            int len=stream_inf.length();

            while(len>0 && stream_inf[len-1]==' ')
                len--;

            stream_inf.resize(len);
        }

        items.push_back(chunk());

        chunk& c=items.back();

        c.seq=seq++;
        c.duration=atof(duration.c_str())*1000;
        c.inf.swap(stream_inf);
        c.url.swap(url);

        duration.clear();

        if(count>=config::hls_chunks_max)
            items.pop_front();
        else
            count++;
    }

    st=0; what=0; tmp.clear();
}

void hls::parser::parse(const char* p,int len)
{
    for(int i=0;i<len;i++)
    {
        int ch=((unsigned char*)p)[i];

        if(ch=='\r')
            continue;
        else if(ch=='\n')
        {
            if(what==3 && !tmp.empty())
                seq=atoi(tmp.c_str());

            push();

            continue;
        }

        switch(st)
        {
        case 0:
            if(ch==' ')
                continue;
            else if(ch=='#')
                { st=1; continue; }
            else
                { url+=ch; st=2; what=1; }
            break;
        case 1:
            if(ch==':')
            {
                if(tmp=="EXTINF")
                    { st=2; what=2; }
                else if(tmp=="EXT-X-MEDIA-SEQUENCE")
                    { st=2; what=3; }
                else if(tmp=="EXT-X-STREAM-INF")
                    { st=2; what=4; }

                tmp.clear();
            }else
                tmp+=ch;
            break;
        case 2:
            if(ch==' ')
                continue;
            else
                st=3;
        case 3:
            if((what!=4 && ch==' ') || (what==2 && ch==','))
                st=4;
            else
            {
                switch(what)
                {
                case 1: url+=ch; break;
                case 2: duration+=ch; break;
                case 3: tmp+=ch; break;
                case 4: stream_inf+=ch; break;
                }
            }
            break;
        }
    }
}

bool hls::req_pls::get_chunklist_url(const std::string& url,int stream_id,std::string& s)
{
    if(open(url) && perform())
    {
        if(!master())
            s.assign(url);
        else
        {
            for(std::list<hls::chunk>::iterator it=items().begin();it!=items().end();++it)
            {
                s.swap(it->url);

                if(stream_id<100)
                {
                    if(stream_id==it->seq)
                        break;
                }else
                {
                    // stream_id>99 содержит желаемое разрешение: 144, 240, 360, 480, 720, 1080

                    const std::string& s=it->inf;

                    std::string::size_type n=s.find("RESOLUTION=");

                    if(n!=std::string::npos)
                    {
                        n+=11;

                        std::string::size_type m=s.find(',',n);

                        std::string ss;

                        if(m!=std::string::npos)
                            ss=s.substr(n,m-n);
                        else
                            ss=s.substr(n);

                        n=ss.find('x');

                        if(n!=std::string::npos && atoi(ss.c_str()+n+1)==stream_id)
                            break;
                    }
                }
            }
        }
    }

    return s.empty()?false:true;
}

std::string hls::req_pls::get_absolute_url(const std::string& s)
{
    if(s.empty() || s.find("://")!=std::string::npos)
        return s;

    std::string ss;

    if(https)
        ss="https://";
    else
        ss="http://";

    ss+=host_name;

    if(s[0]=='/')
        ss+=s;
    else
        { ss+=path; ss+=s; }

    return ss;
}

std::string hls::get_chunklist_url(const std::string& url,int stream_id)
{
    req_pls req;

    std::string s;

    if(req.get_chunklist_url(url,stream_id,s))
        return req.get_absolute_url(s);

    return std::string();
}

bool hls::get_next_chunk(const std::string& url,chunk& c)
{
    req_pls req;

    if(!req.open(url) || !req.perform() || req.master() || req.items().empty())
        return false;

    chunk& cc=req.items().back();

    if(c.url.empty() || cc.seq>c.seq)
    {
        c.seq=cc.seq;

        c.duration=cc.duration;

        c.url=req.get_absolute_url(cc.url);

        return true;
    }

    return false;
}

bool hls::get_chunks(const std::string& url,std::list<chunk>& chunks,int last)
{
    req_pls req;

    if(!req.open(url) || !req.perform() || req.master() || req.items().empty())
        return chunks.empty()?false:true;

    for(std::list<chunk>::iterator it=req.items().begin();it!=req.items().end();++it)
    {
        if(it->seq>last)
        {
            chunks.push_back(chunk());

            chunk& c=chunks.back();

            c.seq=it->seq;

            c.duration=it->duration;

            c.url=req.get_absolute_url(it->url);
        }
    }

    return chunks.empty()?false:true;
}

bool hls::transfer_chunk(const std::string& url,int fd)
{
    req_chunk req(fd);

    if(!req.open(url) || !req.perform())
        return false;

    if(req.content_length>0 && req.content_length!=req.bytes_sent)
        return false;

    return true;
}

/*
int main(void)
{
    curl::context context;

//    std::string url=hls::get_chunklist_url("http://62.113.210.250/medienasa-live/_definst_/mp4:tvhalle_high/playlist.m3u8",0);
    std::string url=hls::get_chunklist_url("http://sochi-strk.ru:1936/strk/strk.stream/playlist.m3u8",720);

    fprintf(stderr,"source: %s\n",url.c_str());

    std::list<hls::chunk> chunks;

    int last=0;

    for(;;)
    {
        hls::timeval tv;

        hls::get_chunks(url,chunks,last);

        if(chunks.empty())
            { usleep(500000); continue; }

        last=chunks.back().seq;

        hls::chunk& c=chunks.front();

        fprintf(stderr,"found fragment %i, duration=%i, url=%s\n",c.seq,c.duration,c.url.c_str());

        hls::transfer_chunk(c.url,1);

        unsigned long elapsed=tv.elapsed_ms();

        unsigned long duration=c.duration;

        chunks.pop_front();

        if(chunks.empty() && elapsed<duration)
            usleep((duration-elapsed)*1000);
    }

    return 0;
}
*/
