CFLAGS          = -fno-exceptions -fno-rtti -Os -I../stlport -I../lua-5.1.5 $(MY_CFLAGS)
OBJS            = main.o m3u8.o curl.o hls.o ev_base.o ev_epoll.o ev_controller.o plugin_broadcast.o plugin_udp.o plugin_hls.o \
 plugin_lua.o ../compat.o
LIBS            = ../lua-5.1.5/liblua.a

all: $(LIBS) $(OBJS)
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CC) -B $(LIBEXEC) -o $(TARGET_DIR)/xupnpd-ottcast $(OBJS) $(LIBS) -ldl -lm
	$(STRIP) $(TARGET_DIR)/xupnpd-ottcast

../lua-5.1.5/liblua.a:
	$(MAKE) -C ../lua-5.1.5 a CC=$(CC) PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) MYCFLAGS="-DLUA_USE_LINUX -Os"

clean:
	$(RM) -f $(OBJS)
	$(MAKE) -C ../lua-5.1.5 clean
	$(RM) -f $(TARGET_DIR)/xupnpd-ottcast

.c.o:
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CC) -c -o $@ $<

.cpp.o:
	PATH=$(PATH):$(LIBEXEC) STAGING_DIR=$(STAGING_DIR) $(CPP) -c $(CFLAGS) -o $@ $<
