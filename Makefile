# Copyright 2013-2018 René Ladan
# SPDX-License-Identifier: BSD-2-Clause

.PHONY: all clean install install-strip doxygen install-doxygen uninstall \
	uninstall-doxygen cppcheck iwyu

PREFIX?=.
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -D_POSIX_C_SOURCE=200809L -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" \
	-g -std=c99
INSTALL?=install
INSTALL_PROGRAM?=$(INSTALL)
CPPCHECK_ARGS?=--enable=all --inconclusive --language=c --std=c99 \
	-DETCDIR=\"$(ETCDIR)\"
# $(shell ...) does not work with FreeBSD make
JSON_C?=`pkg-config --cflags json-c`
JSON_L?=`pkg-config --libs json-c`

all: libdcf77.so dcf77pi dcf77pi-analyze readpin

hdrlib=input.h decode_time.h decode_alarm.h setclock.h mainloop.h \
	bits1to14.h calendar.h
srclib=${hdrlib:.h=.c}
objlib=${hdrlib:.h=.o}
objbin=dcf77pi.o dcf77pi-analyze.o readpin.o

input.o: input.h
	$(CC) -fpic $(CFLAGS) $(JSON_C) -c input.c -o $@
decode_time.o: decode_time.h calendar.h
	$(CC) -fpic $(CFLAGS) -c decode_time.c -o $@
decode_alarm.o: decode_alarm.h
	$(CC) -fpic $(CFLAGS) -c decode_alarm.c -o $@
setclock.o: setclock.h decode_time.h input.h calendar.h
	$(CC) -fpic $(CFLAGS) -c setclock.c -o $@
mainloop.o: mainloop.h input.h bits1to14.h decode_alarm.h decode_time.h \
	setclock.h
	$(CC) -fpic $(CFLAGS) -c mainloop.c -o $@
bits1to14.o: bits1to14.h input.h
	$(CC) -fpic $(CFLAGS) -c bits1to14.c -o $@
calendar.o: calendar.h
	$(CC) -fpic $(CFLAGS) -c calendar.c -o $@

libdcf77.so: $(objlib)
	$(CC) -shared -o $@ $(objlib) -lm -lpthread -lrt $(JSON_L)

dcf77pi.o: bits1to14.h decode_alarm.h decode_time.h input.h \
	mainloop.h calendar.h
	$(CC) -fpic $(CFLAGS) $(JSON_C) -c dcf77pi.c -o $@
dcf77pi: dcf77pi.o libdcf77.so
	$(CC) -o $@ dcf77pi.o -lncurses libdcf77.so $(JSON_L)

dcf77pi-analyze.o: bits1to14.h decode_alarm.h decode_time.h input.h \
	mainloop.h calendar.h
dcf77pi-analyze: dcf77pi-analyze.o libdcf77.so
	$(CC) -fpic $(CFLAGS) -c dcf77pi-analyze.c -o $@
	$(CC) -o $@ dcf77pi-analyze.o libdcf77.so

readpin.o: input.h
	$(CC) -fpic $(CFLAGS) $(JSON_C) -c readpin.c -o $@
readpin: readpin.o libdcf77.so
	$(CC) -o $@ readpin.o libdcf77.so $(JSON_L)

doxygen:
	doxygen

clean:
	rm -f dcf77pi
	rm -f dcf77pi-analyze
	rm -f readpin
	rm -f $(objbin)
	rm -f libdcf77.so $(objlib)

install: libdcf77.so dcf77pi dcf77pi-analyze readpin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	$(INSTALL_PROGRAM) libdcf77.so $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_PROGRAM) dcf77pi dcf77pi-analyze readpin \
		$(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/include/dcf77pi
	$(INSTALL) -m 0644 $(hdrlib) $(DESTDIR)$(PREFIX)/include/dcf77pi
	mkdir -p $(DESTDIR)$(PREFIX)/$(ETCDIR)
	$(INSTALL) -m 0644 etc/dcf77pi/config.json \
		$(DESTDIR)$(PREFIX)/$(ETCDIR)/config.json.sample
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/dcf77pi
	$(INSTALL) -m 0644 LICENSE.md \
		$(DESTDIR)$(PREFIX)/share/doc/dcf77pi

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

install-doxygen: html
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/dcf77pi
	cp -R html $(DESTDIR)$(PREFIX)/share/doc/dcf77pi

install-md:
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/dcf77pi
	$(INSTALL) -m 0644 *.md \
		$(DESTDIR)$(PREFIX)/share/doc/dcf77pi

uninstall: uninstall-doxygen uninstall-md
	rm -f $(DESTDIR)$(PREFIX)/lib/libdcf77.so
	rm -f $(DESTDIR)$(PREFIX)/bin/dcf77pi
	rm -f $(DESTDIR)$(PREFIX)/bin/dcf77pi-analyze
	rm -f $(DESTDIR)$(PREFIX)/bin/readpin
	rm -rf $(DESTDIR)$(PREFIX)/include/dcf77pi
	rm -rf $(DESTDIR)$(PREFIX)/$(ETCDIR)
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/dcf77pi

uninstall-doxygen:
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/dcf77pi/html

uninstall-md:
	rm -f $(DESTDIR)$(PREFIX)/share/doc/dcf77pi/*.md

cppcheck:
	cppcheck -D__CYGWIN__ $(CPPCHECK_ARGS) . || true
	cppcheck -D__linux__ $(CPPCHECK_ARGS) . || true
	cppcheck -D__FreeBSD__ -D__FreeBSD_version=900022 \
		$(CPPCHECK_ARGS) . || true

iwyu:
	$(MAKE) -k CC=include-what-you-use $(objlib) $(objbin) || true
