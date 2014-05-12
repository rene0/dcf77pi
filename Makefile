.PHONY: all clean install install-strip uninstall lint

PREFIX?=.
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" -g
INSTALL_PROGRAM?=install

all: libdcf77.so dcf77pi dcf77pi-analyze readpin

#XXX keep setclock in lib ?

hdrlib = input.h decode_time.h decode_alarm.h config.h
srclib = input.c decode_time.c decode_alarm.c config.c
objlib = input.o decode_time.o decode_alarm.o config.o

hdrgui = guifuncs.h setclock.h
srcgui = guifuncs.c setclock.c dcf77pi.c
objgui = guifuncs.o setclock.o dcf77pi.o

hdrfile =
srcfile = dcf77pi-analyze.c
objfile = dcf77pi-analyze.o

input.o: input.h config.h guifuncs.h
	$(CC) -fPIC -c $< -o $@
decode_time.o: decode_time.h config.h
	$(CC) -fPIC -c $< -o $@
decode_alarm.o: decode_alarm.h
	$(CC) -fPIC -c $< -o $@
config.o: config.h
	$(CC) -fPIC -c $< -o $@
setclock.o: setclock.h
	#$(CC) -fPIC -c $< -o $@

libdcf77.so: $(objlib) $(hdrlib)
	$(CC) -shared -o $@ $(objlib) -lm

dcf77pi.o: $(hdrgui) $(hdrlib)
dcf77pi: $(objgui)
	$(CC) -o $@ $(objgui) -lncurses -ldcf77 -L.

dcf77pi-analyze.o: $(hdrfile) $(hdrlib)
dcf77pi-analyze: $(objfile)
	$(CC) -o $@ $(objfile) guifuncs.o -lncurses -ldcf77 -L.

readpin.o: input.h
readpin: readpin.o
	$(CC) -o $@ readpin.o guifuncs.o -lrt -lm -lncurses -ldcf77 -L.

clean:
	rm dcf77pi $(objgui)
	rm dcf77pi-analyze $(objfile)
	rm readpin readpin.o
	rm libdcf77.so $(objlib)

install: libdcf77.so dcf77pi dcf77pi-analyze readpin
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	$(INSTALL_LIB) libdcf77.so $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	$(INSTALL_PROGRAM) dcf77pi dcf77pi-analyze readpin $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/$(ETCDIR)
	install etc/dcf77pi/config.txt $(DESTDIR)$(PREFIX)/$(ETCDIR)/config.txt.sample

install-strip:
	$(MAKE) INSTALL_PROGRAM='install -s' install

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/lib
	rm -rf $(DESTDIR)$(PREFIX)/bin
	rm -rf $(DESTDIR)$(PREFIX)/$(ETCDIR)

lint:
	lint -aabcehrsxgz -D__linux__ -DETCDIR=\"$(ETCDIR)\" $(srclib) $(srcfile) $(srcgui) readpin.c
	lint -aabcehrsxgz -D__FreeBSD__ -D__FreeBSD_version=900022 -DETCDIR=\"$(ETCDIR)\" $(srclib) $(srcfile) $(srcgui) readpin.c
