# 解压libwebsockets组件源码并完成编译和安装

LIB_TAR := libwebsockets.tar.gz

OUTPUT_DIR = $(PWD)/build

pre_make:
	tar zxvf $(LIB_TAR)

make:
	cd libwebsockets \
	&& mkdir -p build \
	&& cd build \
	&& cmake .. \
		-DLWS_WITH_HTTP2=1 \
		-DLWS_OPENSSL_INCLUDE_DIRS=$(OUTPUT_DIR)/include/ \
		-DLWS_OPENSSL_LIBRARIES="$(OUTPUT_DIR)/lib/libssl.so;$(OUTPUT_DIR)/lib/libcrypto.so" \
		-Wno-error=unused-label \
	&& make

install:
	cp libwebsockets/build/lib/libwebsockets.a $(OUTPUT_DIR)/lib
	cp libwebsockets/build/lib/libwebsockets.so $(OUTPUT_DIR)/lib
	cp libwebsockets/build/lib/libwebsockets.so.12 $(OUTPUT_DIR)/lib
	cp libwebsockets/build/include/*.h $(OUTPUT_DIR)/include/
