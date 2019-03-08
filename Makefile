# inject compile tool-chain when cross complile sdk
#export HOST="x86_64"
#export ARCH="arm-linux-gnueabihf"
#export TARGET=${ARCH}

#export CROSS_ROOT= #the root dir of cross compile tool chain
#export CROSS_COMPILE=${CROSS_ROOT}/bin/arm-linux-gnueabihf-
#export CC=${CROSS_COMPILE}gcc
#export CXX=${CROSS_COMPILE}g++
#export LD=${CROSS_COMPILE}ld
#export AR=${CROSS_COMPILE}ar
#export RANLIB=${CROSS_COMPILE}ranlib
#export STRIP=${CROSS_COMPILE}strip

# default compile output
all :
	$(MAKE) leda_sdk_c
	$(MAKE) -C demo -f demo.mk

# deps libary
prepare :
	$(MAKE) -C deps -f cjson.mk
	$(MAKE) -C deps -f expat.mk
	$(MAKE) -C deps -f dbus-1.mk

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
distclean: clean
	-$(RM) -r ./build
	-$(RM) -r ./deps/cJSON-master/
	-$(RM) -r ./deps/dbus-1.10.18/
	-$(RM) -r ./deps/libexpat/

.PHONY: deps demo src
