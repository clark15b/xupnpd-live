PLATFORM        = x86_64

all:
	g++ -fno-exceptions -fno-rtti -O2 -o xupnpd-ottcast main.cpp m3u8.cpp curl.cpp hls.cpp \
 ev_base.cpp ev_epoll.cpp ev_controller.cpp plugin_broadcast.cpp plugin_udp.cpp plugin_hls.cpp plugin_lua.cpp -llua
	strip xupnpd-ottcast

clean:
	rm -f xupnpd-ottcast