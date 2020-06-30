
CFLAGS  = -g -Wall -O2

INCLUDE_PATH = -I$(PWD)/build/include
INCLUDE      = -I./ $(INCLUDE_PATH)/ $(INCLUDE_PATH)/cjson $(INCLUDE_PATH)/dbus-1.0

LIB_PATH = -L$(PWD)/build/lib
LIB 	 =  -lleda_sdk_c  \
			-lcjson       \
			-lpthread     \
			-ldbus-1

OBJS     = ./lora.o

DRIVER_NAME = demo_lora
DEPENDS     = lib
TARGET      = main
TARGET_ZIP  = lora_driver

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $^ -o $@ $(CFLAGS) $(INCLUDE) $(LIB_PATH) $(LIB) 

$(OBJS):%o:%c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE)

install :
	mkdir -p $(PWD)/build/bin/demo/$(DRIVER_NAME)/
	mkdir -p $(DEPENDS)
	cp -fr $(PWD)/build/lib/* $(DEPENDS)/
	zip -q -r $(TARGET_ZIP).zip $(TARGET) $(DEPENDS)
	cp $(TARGET_ZIP).zip $(PWD)/build/bin/demo/$(DRIVER_NAME)/

clean:
	-$(RM) -r $(TARGET) $(DEPENDS) $(OBJS) $(TARGET_ZIP).zip
