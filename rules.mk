CFLAGS  = -fno-exceptions -fno-rtti -Os -Istlport -Ilua-5.1.5 $(MY_CFLAGS) -DWITH_SSL -Imbedtls/include -Lmbedtls/library
OBJS    = main.o ssdp.o server.o worker.o producer.o m3u8.o curl.o hls.o config.o common.o compat.o soap.o log.o json.o xml.o luaaux.o
LIBS    = lua-5.1.5/liblua.a

all: tls $(LIBS) $(OBJS)
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CC) -B $(LIBEXEC) $(CFLAGS) -o $(TARGET_DIR)/xupnpd-live-$(PLATFORM) $(OBJS) $(LIBS) -ldl -lm -lmbedtls -lmbedx509 -lmbedcrypto
	$(STRIP) $(TARGET_DIR)/xupnpd-live-$(PLATFORM)

lua-5.1.5/liblua.a:
	$(MAKE) -C lua-5.1.5 a CC=$(CC) PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) MYCFLAGS="-DLUA_USE_LINUX -Os"

tls:
	$(MAKE) -C mbedtls CC=$(CC) PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR)

clean:
	$(RM) -f $(OBJS)
	$(MAKE) -C lua-5.1.5 clean
	$(MAKE) -C mbedtls clean
	$(RM) -f $(TARGET_DIR)/xupnpd-live-$(PLATFORM) xupnpd.uid

.c.o:
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CC) -c -o $@ $<

.cpp.o:
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CPP) -c $(CFLAGS) -o $@ $<
