#include <string>
#include <stdlib.h>
#include <string.h>

#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 0 or MBEDTLS_ERR_SSL_WANT_READ/WRITE
// mbedtls_ssl_handshake
// mbedtls_ssl_write
// mbedtls_ssl_read             // MBEDTLS_ERR_SSL_CLIENT_RECONNECT
// mbedtls_ssl_close_notify

namespace curl
{
    class tlsctx
    {
    protected:
        mbedtls_ssl_config conf;

        mbedtls_entropy_context entropy;

        mbedtls_ctr_drbg_context ctr_drbg;

        mbedtls_x509_crt cacert;
    public:
        tlsctx(const std::string& ca_chain)
        {
            static const char custom_seed[]="VerYvEryVErYSEcRetwORd";

            mbedtls_ssl_config_init(&conf);

            mbedtls_ctr_drbg_init(&ctr_drbg);

            mbedtls_entropy_init(&entropy);

            mbedtls_x509_crt_init(&cacert);

            mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,(const unsigned char*)custom_seed,sizeof(custom_seed)-1);

            mbedtls_ssl_config_defaults(&conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);

            mbedtls_ssl_conf_rng(&conf,mbedtls_ctr_drbg_random,&ctr_drbg);

            if(!ca_chain.empty())
            {
                mbedtls_ssl_conf_authmode(&conf,MBEDTLS_SSL_VERIFY_REQUIRED);   // MBEDTLS_SSL_VERIFY_NONE

                if(!mbedtls_x509_crt_parse_file(&cacert,ca_chain.c_str()))
                    mbedtls_ssl_conf_ca_chain(&conf,&cacert,NULL);
            }else
                mbedtls_ssl_conf_authmode(&conf,MBEDTLS_SSL_VERIFY_NONE);
        }

        ~tlsctx(void)
        {
            mbedtls_x509_crt_free(&cacert);

            mbedtls_entropy_free(&entropy);

            mbedtls_ctr_drbg_free(&ctr_drbg);

            mbedtls_ssl_config_free(&conf);
        }

        friend class connection;
    };

    class connection
    {
    protected:
        mbedtls_ssl_context ssl;

        mbedtls_net_context server_fd;
    public:
        connection(tlsctx& ctx)
        {
            mbedtls_net_init(&server_fd);

            mbedtls_ssl_init(&ssl);

            mbedtls_ssl_setup(&ssl,&ctx.conf);
        }

        ~connection(void)
        {
            mbedtls_ssl_close_notify(&ssl);

            mbedtls_ssl_free(&ssl);

            mbedtls_net_free(&server_fd);
        }

        bool connect(const std::string& ip,const std::string hostname)
        {
            if(!hostname.empty())
                mbedtls_ssl_set_hostname(&ssl,hostname.c_str());

            sockaddr_in sin;
            sin.sin_family=AF_INET;
            sin.sin_addr.s_addr=inet_addr("80.211.17.188");
            sin.sin_port=htons(443);
#if defined(__FreeBSD__) || defined(__APPLE__)
            sin.sin_len=sizeof(sockaddr_in);
#endif

            server_fd.fd=socket(PF_INET,SOCK_STREAM,0);

            if(server_fd.fd!=-1)
            {
                if(!::connect(server_fd.fd,(sockaddr*)&sin,sizeof(sin)))
                {
                    mbedtls_ssl_set_bio(&ssl,&server_fd,mbedtls_net_send,mbedtls_net_recv,NULL);

                    if(!mbedtls_ssl_handshake(&ssl))
                        return true;
                }
            }

            return false;
        }

        void test(void)
        {
            const char req[]="GET / HTTP/1.0\r\n\r\n";

            if(mbedtls_ssl_write(&ssl,(const unsigned char*)req,sizeof(req)-1)==sizeof(req)-1)
            {
                char buf[512];

                int n;

                while((n=mbedtls_ssl_read(&ssl,(unsigned char*)buf,sizeof(buf)))>0)
                    fwrite(buf,n,1,stdout);
            }
        }
    };
}


int main( void )
{
    curl::tlsctx ctx("cacert.pem");

    {
        curl::connection ssl(ctx);

        if(ssl.connect("87.250.250.242","ya.ru"))
            ssl.test();
    }

    return 0;
}
