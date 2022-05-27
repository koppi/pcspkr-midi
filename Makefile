# install prefix either /usr or /usr/local on most unix systems
PREFIX ?= /usr

LDLIBS=-lasound -lm

all: pcspkr-midi

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $+

pcspkr-midi: pcspkr-midi.o
	$(CC) -o $@ $+ $(LDFLAGS) $(LDLIBS)

install: pcspkr-midi
	strip pcspkr-midi
	mkdir -p $(INSTALL_PREFIX)$(PREFIX)/bin
	install -m 0755 pcspkr-midi $(INSTALL_PREFIX)$(PREFIX)/bin

uninstall:
	rm $(INSTALL_PREFIX)$(PREFIX)/bin/pcspkr-midi

pull:
	git pull origin main --rebase

push:
	git push origin HEAD:main

clean:
	rm -f pcspkr-midi *.o *~
