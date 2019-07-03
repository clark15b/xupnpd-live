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

int main( void )
{
    const char* hostname="ya.ru";

    mbedtls_net_context server_fd;

    mbedtls_x509_crt cacert;

    mbedtls_entropy_context entropy;

    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_ssl_context ssl;

    mbedtls_ssl_config conf;

    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);

    mbedtls_entropy_init(&entropy);

    if(!mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,(const unsigned char*)"test",4))
    {
        if(!mbedtls_ssl_config_defaults(&conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT))
        {
            mbedtls_ssl_conf_rng(&conf,mbedtls_ctr_drbg_random,&ctr_drbg);

            if(!mbedtls_x509_crt_parse_file(&cacert,"cacert.pem"))
            {
                mbedtls_ssl_conf_ca_chain(&conf,&cacert,NULL);
                mbedtls_ssl_conf_authmode(&conf,MBEDTLS_SSL_VERIFY_REQUIRED);

                if(!mbedtls_ssl_setup(&ssl,&conf))
                {
                    if(!mbedtls_ssl_set_hostname(&ssl,hostname))
                    {
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
                            if(!connect(server_fd.fd,(sockaddr*)&sin,sizeof(sin)))
                            {
                                mbedtls_ssl_set_bio(&ssl,&server_fd,mbedtls_net_send,mbedtls_net_recv,NULL);

                                if(!mbedtls_ssl_handshake(&ssl))
                                {
                                    const char req[]="GET / HTTP/1.0\r\n\r\n";

                                    if(mbedtls_ssl_write(&ssl,(const unsigned char*)req,sizeof(req)-1)==sizeof(req)-1)
                                    {
                                        char buf[512];

                                        int n;

                                        while((n=mbedtls_ssl_read(&ssl,(unsigned char*)buf,sizeof(buf)))>0)
                                            fwrite(buf,n,1,stdout);
                                    }

                                    mbedtls_ssl_close_notify(&ssl);
                                }
                            }

                            mbedtls_net_free(&server_fd);
                        }
                    }
                }

                mbedtls_x509_crt_free(&cacert);
            }

            mbedtls_ssl_config_free( &conf );
        }

        mbedtls_ctr_drbg_free(&ctr_drbg);
    }

    mbedtls_entropy_free(&entropy);

    mbedtls_ssl_free(&ssl);

    return 0;
}
