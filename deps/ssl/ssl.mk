
OUTPUT_DIR = $(PWD)/build
LIB_TAR = openssl-1.0.2p

mk_path := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))

pre_make:
	tar zxf $(LIB_TAR).tar.gz
	cp openssl.map ./$(LIB_TAR)
	cd ./$(LIB_TAR) \
		&& ./Configure linux-generic64 \
		shared \
		threads \
		--prefix=$(OUTPUT_DIR) \
		-Wl,--version-script=./openssl.map

make:
	cd ./$(LIB_TAR) && make -j8 build_libs build_apps

install:
	cd ./$(LIB_TAR) && make install_sw 

clean:
	cd ./$(LIB_TAR) && make clean
	rm -rf ./$(LIB_TAR)
