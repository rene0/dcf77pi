.PHONY: all clean install install-strip uninstall lint

PREFIX?=.
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" -g
INSTALL_PROGRAM?=install

all: libdcf77.so dcf77pi dcf77pi-analyze readpin

hdrlib = input.h decode_time.h decode_alarm.h config.h setclock.h mainloop.h \
	bits1to14.h
srclib = input.c decode_time.c decode_alarm.c config.c setclock.c mainloop.c \
	bits1to14.c
objlib = input.o decode_time.o decode_alarm.o config.o setclock.o mainloop.o \
	bits1to14.o

srcgui = dcf77pi.c
objgui = dcf77pi.o

srcfile = dcf77pi-analyze.c
objfile = dcf77pi-analyze.o

input.o: input.h config.h
	$(CC) -fPIC $(CFLAGS) -c input.c -o $@
decode_time.o: decode_time.h config.h
	$(CC) -fPIC $(CFLAGS) -c decode_time.c -o $@
decode_alarm.o: decode_alarm.h
	$(CC) -fPIC $(CFLAGS) -c decode_alarm.c -o $@
config.o: config.h
	$(CC) -fPIC $(CFLAGS) -c config.c -o $@
setclock.o: setclock.h
	$(CC) -fPIC $(CFLAGS) -c setclock.c -o $@
mainloop.o: mainloop.h
	$(CC) -fPIC $(CFLAGS) -c mainloop.c -o $@
bits1to14.o: bits1to14.h
	$(CC) -fPIC $(CFLAGS) -c bits1to14.c -o $@

libdcf77.so: $(objlib) $(hdrlib)
	$(CC) -shared -o $@ $(objlib) -lm

dcf77pi.o: $(hdrlib)
dcf77pi: $(objgui) libdcf77.so
	$(CC) -o $@ $(objgui) -lncurses libdcf77.so

dcf77pi-analyze.o: $(hdrlib)
dcf77pi-analyze: $(objfile) libdcf77.so
	$(CC) -o $@ $(objfile) libdcf77.so

readpin.o: input.h
readpin: readpin.o libdcf77.so
	$(CC) -o $@ readpin.o -lm libdcf77.so

clean:
	rm dcf77pi $(objgui)
	rm dcf77pi-analyze $(objfile)
	rm readpin readpin.o
	rm libdcf77.so $(objlib)

install: libdcf77.so dcf77pi dcf77pi-analyze readpin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	$(INSTALL_PROGRAM) libdcf77.so $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_PROGRAM) dcf77pi dcf77pi-analyze readpin $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/include/dcf77pi
	$(INSTALL) $(hdrlib) $(DESTDIR)$(PREFIX)/include/dcf77pi
	mkdir -p $(DESTDIR)$(PREFIX)/$(ETCDIR)
	install etc/dcf77pi/config.txt $(DESTDIR)$(PREFIX)/$(ETCDIR)/config.txt.sample

install-strip:
	$(MAKE) INSTALL_PROGRAM='install -s' install

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/lib
	rm -rf $(DESTDIR)$(PREFIX)/bin
	rm -rf $(DESTDIR)$(PREFIX)/include/dcf77pi
	rm -rf $(DESTDIR)$(PREFIX)/$(ETCDIR)

lint:
	lint -aabcehrsxgz -D__linux__ -DETCDIR=\"$(ETCDIR)\" $(srclib) $(srcfile) readpin.c $(srcgui) || true
	lint -aabcehrsxgz -D__FreeBSD__ -D__FreeBSD_version=900022 -DETCDIR=\"$(ETCDIR)\" $(srclib) $(srcfile) readpin.c $(srcgui) || true
