

CFLAGS  = -g -Wall -O2 

OBJS = ./log.o

SHARE_LIB   = liblogger.so

INCLUDE_PATH =  -I$(PWD)/build/include
INCLUDE      =  -I./ -I./include $(INCLUDE_PATH) $(INCLUDE_PATH)/cjson $(INCLUDE_PATH)/dbus-1.0 

all : $(SHARE_LIB) install

$(OBJS):%o:%c
	$(CC) -c -fPIC $< -o $@ $(CFLAGS) $(INCLUDE)

$(SHARE_LIB) : $(OBJS)
	-$(RM) $@
	$(CC) -shared -o -fPIC -o $@ $^

install:
	mkdir -p $(PWD)/build/lib/
	cp $(SHARE_LIB) $(PWD)/build/lib/
	cp ./*.h $(PWD)/build/include/

clean:
	-$(RM) $(OBJS) $(SHARE_LIB)
