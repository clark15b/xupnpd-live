PLATFORM        = x86_64

all:
	$(MAKE) -C mbedtls
	g++ -DWITH_SSL -Imbedtls/include -Lmbedtls/library -I. -o xupnpd-live main.cpp ssdp.cpp server.cpp worker.cpp producer.cpp m3u8.cpp curl.cpp hls.cpp config.cpp common.cpp soap.cpp log.cpp json.cpp xml.cpp luaaux.cpp -llua -lmbedtls -lmbedx509 -lmbedcrypto

clean:
	$(MAKE) -C mbedtls clean
	rm -f xupnpd-live xupnpd.uid