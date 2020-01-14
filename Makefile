PLATFORM        = x86_64
LUA             = lua-5.3.5
LIBS            = $(LUA)/liblua.a

all: $(LIBS)
	$(MAKE) -C mbedtls
	g++ -DWITH_SSL -Imbedtls/include -Lmbedtls/library -I$(LUA) -I. -o xupnpd-live main.cpp ssdp.cpp server.cpp worker.cpp producer.cpp m3u8.cpp curl.cpp hls.cpp config.cpp common.cpp soap.cpp log.cpp json.cpp xml.cpp luaaux.cpp -lmbedtls -lmbedx509 -lmbedcrypto -ldl $(LIBS)

$(LUA)/liblua.a:
	$(MAKE) -C $(LUA) a CC=$(CC) PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) MYCFLAGS="-DLUA_USE_LINUX -O2"

clean:
	$(MAKE) -C mbedtls clean
	$(MAKE) -C $(LUA) clean
	rm -f xupnpd-live xupnpd.uid