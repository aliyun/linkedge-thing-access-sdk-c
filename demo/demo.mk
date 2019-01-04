
all :
	mkdir -p $(PWD)/build/bin/demo/
	$(MAKE) -C led -f led.mk

install:
	$(MAKE) -C led -f led.mk install

clean:
	$(MAKE) -C led -f led.mk clean
