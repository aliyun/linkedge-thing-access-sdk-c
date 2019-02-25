# 解压libexpat2.2.0组件源码并完成编译和安装

LIB_TAR    = libexpat
OUTPUT_DIR = $(PWD)/build

all :
	tar xvf ./$(LIB_TAR)-fbc46fa.tar.bz2

	cd ${LIB_TAR}/expat \
		&& ./buildconf.sh \
		&& ./configure \
			--prefix=$(OUTPUT_DIR) \
			--enable-shared \
			--host=${ARCH} \
		&& make buildlib \
		&& make installlib

clean :
	cd $(LIB_TAR) && make clean
