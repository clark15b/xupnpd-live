#include <stdio.h>
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"

// https://tls.mbed.org/kb/how-to/mbedtls-tutorial

void my_debug(void* ctx,int level,const char* file,int line,const char* str)
{
    printf("%s\n",str);
}

int main(void)
{
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_x509_crt cacert;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;

    mbedtls_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_x509_crt_init(&cacert);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    mbedtls_entropy_init(&entropy);

    int ret;

    if((ret=mbedtls_ctr_drbg_seed(&ctr_drbg,mbedtls_entropy_func,&entropy,(const unsigned char*)"5ihexmpormtoihfr9024jxml",24))!=0)
    {
        printf(" failed\n  ! mbedtls_ctr_drbg_seed returned %d\n",ret);
    }

    if((ret=mbedtls_ssl_config_defaults(&conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT))!=0)
    {
        printf(" failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n",ret);
    }

    mbedtls_ssl_conf_rng(&conf,mbedtls_ctr_drbg_random,&ctr_drbg);

    mbedtls_ssl_conf_dbg(&conf,my_debug,stdout);
    mbedtls_debug_set_threshold(0);

    mbedtls_ssl_conf_authmode(&conf,MBEDTLS_SSL_VERIFY_REQUIRED);
//    mbedtls_ssl_conf_authmode(&conf,MBEDTLS_SSL_VERIFY_NONE);

    if((ret=mbedtls_x509_crt_parse_file(&cacert,"cacert.pem"))!=0)
    {
        printf(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x\n\n",-ret);
    }

//    if((ret=mbedtls_x509_crt_parse(&cacert,(const unsigned char*)SSL_CA_PEM,sizeof(SSL_CA_PEM)))!=0)
//    {
//        printf("mbedtls_x509_crt_parse",ret);
//    }

    mbedtls_ssl_conf_ca_chain(&conf,&cacert,NULL);

//    if((ret=mbedtls_ssl_setup(&ssl,&conf))!=0)
//    {
//        printf("mbedtls_ssl_setup", ret);
//    }

    if((ret=mbedtls_net_connect(&server_fd,"ya.ru","443",MBEDTLS_NET_PROTO_TCP))!=0)
    {
        printf(" failed\n  ! mbedtls_net_connect returned %d\n\n",ret);
    }


    char buf[512];
    mbedtls_x509_crt_info(buf,sizeof(buf), "\r    ",mbedtls_ssl_get_peer_cert(&ssl));
    printf("Server certificate:\r\n%s\r",buf);

    mbedtls_net_free(&server_fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return 0;
}
