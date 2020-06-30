
all :
	mkdir -p $(PWD)/build/bin/demo/
	$(MAKE) -C led -f led.mk
	$(MAKE) -C lora -f lora.mk

install:
	$(MAKE) -C led -f led.mk install
	$(MAKE) -C lora -f lora.mk install

clean:
	$(MAKE) -C led -f led.mk clean
	$(MAKE) -C lora -f lora.mk clean
