CC=gcc
CFLAGS=-Wall
LDFLAGS=-levdev

ifeq ($(DEBUG), 1)
  CFLAGS += -DDEBUG=1
endif

all: nintendo-xinput

nintendo-xinput: main.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: nintendo-xinput
	cp nintendo-xinput /usr/bin/

uninstall:
	rm -f /usr/bin/nintendo-xinput

clean:
	rm -f nintendo-xinput
