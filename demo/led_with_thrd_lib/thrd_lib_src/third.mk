
CFLAGS  = -g -Wall -O2 

OBJS = ./third.o

SHARE_LIB   = libthird.so

INCLUDE      = -I./

all : $(SHARE_LIB) install

$(OBJS):%o:%c
	$(CC) -c -fPIC $< -o $@ $(CFLAGS) $(INCLUDE)

$(SHARE_LIB) : $(OBJS)
	-$(RM) $@
	$(CC) -shared -o -fPIC -o $@ $^

install:
	mkdir -p ../lib/ && cp $(SHARE_LIB) ../lib/
	mkdir -p ../include && cp ./*.h ../include/

clean:
	-$(RM) $(OBJS) $(SHARE_LIB)
	rm -fr ../include
	rm -fr ../lib
