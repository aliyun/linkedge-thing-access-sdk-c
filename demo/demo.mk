
all :
	mkdir -p $(PWD)/build/bin/demo/
	$(MAKE) -C led -f led.mk
	$(MAKE) -C led_with_thrd_lib -f led.mk

install:
	$(MAKE) -C led -f led.mk install
	$(MAKE) -C led_with_thrd_lib -f led.mk install

clean:
	$(MAKE) -C led -f led.mk clean
	$(MAKE) -C led_with_thrd_lib -f led.mk clean
