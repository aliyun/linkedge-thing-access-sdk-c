# 解压cJson组件源码并完成编译和安装

LIB_TAR    = cJSON-1.7.7
OUTPUT_DIR = $(PWD)/build

all :
	tar xvfz ./$(LIB_TAR).tar.gz
	cp cjson.patch $(LIB_TAR)
	cd $(LIB_TAR) && git apply cjson.patch && make clean && make shared && make DESTDIR=$(OUTPUT_DIR) install

clean :
	cd $(LIB_TAR) && make clean