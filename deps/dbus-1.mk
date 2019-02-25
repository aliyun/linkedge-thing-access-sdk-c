# 解压dbus组件源码并完成编译和安装

LIB_TAR    = dbus-1.10.18
OUTPUT_DIR = $(PWD)/build

all :
	tar zxvf ./$(LIB_TAR).tar.gz
	cd $(LIB_TAR) && CFLAGS=-I${OUTPUT_DIR}/include LDFLAGS=-L${OUTPUT_DIR}/lib ./configure prefix=$(OUTPUT_DIR) \
			--host=${ARCH} \
			--enable-abstract-sockets \
			--disable-tests \
		&& make && cd -
	cd $(LIB_TAR) && make install && cd - && rm -rf $(OUTPUT_DIR)/libexec/ $(OUTPUT_DIR)/var/ $(OUTPUT_DIR)/etc/ $(OUTPUT_DIR)/share/
	cd $(OUTPUT_DIR)/bin/ && rm -f dbus-launch dbus-test-tool dbus-monitor dbus-update-activation-environment dbus-run-session dbus-uuidgen dbus-cleanup-sockets dbus-send && cd -
	cp $(OUTPUT_DIR)/lib/dbus-1.0/include/dbus/dbus-arch-deps.h $(OUTPUT_DIR)/include/dbus-1.0/dbus/

clean :
	cd $(LIB_TAR) && make clean
