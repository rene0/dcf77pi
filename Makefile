.PHONY: all clean install install-strip doxygen install-doxygen uninstall \
	uninstall-doxygen lint splint cppcheck iwyu

PREFIX?=.
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" -g
INSTALL?=install
INSTALL_PROGRAM?=$(INSTALL)
LINT_ARGS?=-aabcehrsxS -Dlint -DETCDIR=\"$(ETCDIR)\"
SPLINT_ARGS?=+posixlib -DETCDIR=\"$(ETCDIR)\"
CPPCHECK_ARGS?=--enable=all --inconclusive --language=c --std=c99 \
	-DETCDIR=\"$(ETCDIR)\"

all: libdcf77.so dcf77pi dcf77pi-analyze readpin testcentury

hdrlib=input.h decode_time.h decode_alarm.h config.h setclock.h mainloop.h \
	bits1to14.h calendar.h
srclib=input.c decode_time.c decode_alarm.c config.c setclock.c mainloop.c \
	bits1to14.c calendar.c
srcbin=dcf77pi-analyze.c readpin.c dcf77pi.c testcentury.c
objlib=input.o decode_time.o decode_alarm.o config.o setclock.o mainloop.o \
	bits1to14.o calendar.o
objbin=dcf77pi.o dcf77pi-analyze.o readpin.o testcentury.o

input.o: input.h config.h
	$(CC) -fpic $(CFLAGS) -c input.c -o $@
decode_time.o: decode_time.h config.h calendar.h
	$(CC) -fpic $(CFLAGS) -c decode_time.c -o $@
decode_alarm.o: decode_alarm.h
	$(CC) -fpic $(CFLAGS) -c decode_alarm.c -o $@
config.o: config.h
	$(CC) -fpic $(CFLAGS) -c config.c -o $@
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
	$(CC) -shared -o $@ $(objlib) -lm -lrt

dcf77pi.o: bits1to14.h config.h decode_alarm.h decode_time.h input.h mainloop.h \
	calendar.h
dcf77pi: dcf77pi.o libdcf77.so
	$(CC) -o $@ dcf77pi.o -lncurses libdcf77.so

dcf77pi-analyze.o: bits1to14.h config.h decode_alarm.h decode_time.h input.h \
	mainloop.h calendar.h
dcf77pi-analyze: dcf77pi-analyze.o libdcf77.so
	$(CC) -o $@ dcf77pi-analyze.o libdcf77.so

readpin.o: config.h input.h
readpin: readpin.o libdcf77.so
	$(CC) -o $@ readpin.o libdcf77.so

testcentury.o: calendar.h
testcentury: testcentury.o libdcf77.so
	$(CC) -o $@ testcentury.o libdcf77.so

doxygen:
	doxygen

clean:
	rm -f dcf77pi
	rm -f dcf77pi-analyze
	rm -f readpin
	rm -f testcentury
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
	$(INSTALL) -m 0644 etc/dcf77pi/config.txt \
		$(DESTDIR)$(PREFIX)/$(ETCDIR)/config.txt.sample

install-strip:
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install

install-doxygen: html
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/dcf77pi
	cp -R html $(DESTDIR)$(PREFIX)/share/doc/dcf77pi

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/libdcf77.so
	rm -f $(DESTDIR)$(PREFIX)/bin/dcf77pi
	rm -f $(DESTDIR)$(PREFIX)/bin/dcf77pi-analyze
	rm -f $(DESTDIR)$(PREFIX)/bin/readpin
	rm -rf $(DESTDIR)$(PREFIX)/include/dcf77pi
	rm -rf $(DESTDIR)$(PREFIX)/$(ETCDIR)

uninstall-doxygen:
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/dcf77pi

lint:
	lint -D__CYGWIN__ $(LINT_ARGS) $(srclib) $(srcbin) || true
	lint -D__linux__ $(LINT_ARGS) $(srclib) $(srcbin) || true
	lint -D__FreeBSD__ -D__FreeBSD_version=900022 \
		$(LINT_ARGS) $(srclib) $(srcbin) || true

#XXX no development since 2007-07-12, broken with clang 3.9
splint:
	splint -D__CYGWIN__ $(SPLINT_ARGS) $(srclib) $(srcbin) || true
	splint -D__linux__ $(SPLINT_ARGS) $(srclib) $(srcbin) || true
	splint -D__FreeBSD__ -D__FreeBSD_version=900022 \
		$(SPLINT_ARGS) $(srclib) $(srcbin) || true

cppcheck:
	cppcheck -D__CYGWIN__ $(CPPCHECK_ARGS) . || true
	cppcheck -D__linux__ $(CPPCHECK_ARGS) . || true
	cppcheck -D__FreeBSD__ -D__FreeBSD_version=900022 \
		$(CPPCHECK_ARGS) . || true

iwyu:
	$(MAKE) -k CC=include-what-you-use $(objlib) $(objbin) || true
