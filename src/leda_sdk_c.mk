
CONFIG_MBUS_UNIX_PATH = /tmp/var/run/mbusd/mbusd_socket

CFLAGS  = -g -Wall -O2 
CFLAGS  += -Dbus_address=\"unix:path=$(CONFIG_MBUS_UNIX_PATH)\"

OBJS = ./runtime/websocket_client/ws_client.o \
	   ./runtime/websocket_client/ws_client_network.o \
	   ./runtime/websocket_client/ws_client_callback.o \
	   ./runtime/websocket_client/wsc_buffer_mgmt.o \
	   ./runtime/runtime.o \
	   ./leda_base.o \
	   ./leda_methodcb.o \
	   ./leda_trpool.o \
	   ./leda.o

STATIC_LIB   = ../lib/libleda_sdk_c.a

INCLUDE_PATH =  -I$(PWD)/build/include
INCLUDE      = -I./ -I../include $(INCLUDE_PATH) $(INCLUDE_PATH)/ssl -I./runtime/websocket_client -I./runtime/ $(INCLUDE_PATH)/cjson $(INCLUDE_PATH)/dbus-1.0

all : $(STATIC_LIB) install

$(OBJS):%o:%c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

$(STATIC_LIB) : $(OBJS)
	-$(RM) $@
	mkdir -p ../lib/
	ar cr $@ $(OBJS)

install:
	cp $(STATIC_LIB) $(PWD)/build/lib/
	cp ../include/*.h $(PWD)/build/include/

clean:
	-$(RM) $(OBJS) $(STATIC_LIB)
