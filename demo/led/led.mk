
CFLAGS  = -g -Wall -O2

INCLUDE_PATH = -I$(PWD)/build/include
INCLUDE      = -I./ $(INCLUDE_PATH)/ $(INCLUDE_PATH)/cjson $(INCLUDE_PATH)/dbus-1.0 $(INCLUDE_PATH)/ssl 

LIB_PATH = -L$(PWD)/build/lib
LIB 	 =  -lleda_sdk_c  \
			-lcjson       \
			-lpthread     \
			-ldbus-1

OBJS     = ./src/led.o

DEPENDS  = lib
TARGET   = main
TARGET_ZIP = demo_led

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(INCLUDE) $(LIB_PATH) $(LIB) 

$(OBJS):%o:%c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

install :
	mkdir -p $(PWD)/build/bin/demo/led/
	mkdir -p $(DEPENDS)
	cp -fr $(PWD)/build/lib/* $(DEPENDS)/
	zip -q -r $(TARGET_ZIP).zip $(TARGET) lib
	cp $(TARGET_ZIP).zip $(PWD)/build/bin/demo/led/

clean:
	-$(RM) -r $(TARGET) $(DEPENDS) $(OBJS) $(TARGET_ZIP).zip



