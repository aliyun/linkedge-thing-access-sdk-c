# 解压cJson组件源码并完成编译和安装

LIB_TAR    = cJSON-master
OUTPUT_DIR = $(PWD)/build

all :
	tar xvf ./$(LIB_TAR).tar
	cd $(LIB_TAR) && make clean && make shared && make DESTDIR=$(OUTPUT_DIR) install

clean :
	cd $(LIB_TAR) && make clean