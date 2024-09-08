CC=gcc
CFLAGS=-O2 -pipe
LIBS=-lX11 -lpthread -lm -lXtst
FEATURES=

ifdef FEATURES
    ifneq ($(findstring SMART_TV,$(FEATURES)),)
        CFLAGS += -DSMART_TV
    endif
endif

xboxmouse: xboxmouse.o
	$(CC) $(CFLAGS) $(LIBS) -o xboxmouse xboxmouse.o 

xboxmouse.o: xboxmouse.c
	$(CC) $(CFLAGS) -c xboxmouse.c

clean:
	rm xboxmouse.o
	rm xboxmouse

install:
	cp xboxmouse /usr/local/bin
	chmod 0755 /usr/local/bin/xboxmouse

uninstall:
	rm /usr/local/bin/xboxmouse

