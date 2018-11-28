# default compile output
all :
	$(MAKE) leda_sdk_c
	$(MAKE) -C demo -f demo.mk

# deps libary
prepare :
	$(MAKE) -C deps -f cjson.mk
	$(MAKE) -C deps -f dbus-1.mk
	$(MAKE) -C deps/ssl -f ssl.mk pre_make make install
	$(MAKE) -C deps -f libwebsockets.mk pre_make make install
	$(MAKE) -C utils/log -f log.mk

# linkedge device access sdk
leda_sdk_c :
	$(MAKE) -C src -f leda_sdk_c.mk

# clean tempory compile resource
clean:
	$(MAKE) -C src -f leda_sdk_c.mk clean
	$(MAKE) -C demo -f demo.mk clean

install:
	$(MAKE) -C demo -f demo.mk install

# delete compile resource
remove:
	rm -fr ./build
	rm -fr ./deps/cJSON-master/
	rm -fr ./deps/dbus-1.10.18/
	rm -fr ./deps/ssl/openssl-1.0.2p/

.PHONY: demo src
