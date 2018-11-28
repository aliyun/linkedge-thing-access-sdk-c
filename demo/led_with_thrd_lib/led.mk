
CFLAGS  = -g -Wall -O2

INCLUDE_PATH = -I$(PWD)/build/include
INCLUDE      = -I./ -I./include $(INCLUDE_PATH)/ $(INCLUDE_PATH)/ssl $(INCLUDE_PATH)/cjson $(INCLUDE_PATH)/dbus-1.0

LIB_PATH = -L$(PWD)/build/lib -L./lib
LIB 	 =  -lssl		  \
	  		-lwebsockets  \
	  		-lcrypto	  \
			-lleda_sdk_c  \
			-llogger	  \
			-lcjson       \
			-lpthread     \
			-ldbus-1	  \
			-lthird

OBJS     = ./src/led.o

TARGET   = main
TARGET_ZIP = demo_led

all: prepare_thrd_lib $(TARGET)

prepare_thrd_lib:
	make -C thrd_lib_src -f third.mk

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(INCLUDE) $(LIB_PATH) $(LIB) 

$(OBJS):%o:%c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

install :
	mkdir -p $(PWD)/build/bin/demo/led_with_thrd_lib/
	zip -q -r $(TARGET_ZIP).zip $(TARGET) include lib
	cp $(TARGET_ZIP).zip $(PWD)/build/bin/demo/led_with_thrd_lib/

clean:
	make -C thrd_lib_src -f third.mk clean
	-$(RM) $(TARGET) $(OBJS) $(TARGET_ZIP).zip
