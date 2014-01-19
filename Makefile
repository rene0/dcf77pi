.PHONY: all clean install

PREFIX?=.
FULLPREFIX?=$(PREFIX)
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" -g

all: dcf77pi readpin

hdr = input.h decode_time.h decode_alarm.h config.h
src = dcf77pi.c input.c decode_time.c decode_alarm.c config.c
obj = dcf77pi.o input.o decode_time.o decode_alarm.o config.o

input.o: input.h config.h
decode_time.o: decode_time.h config.h
decode_alarm.o: decode_alarm.h
config.o: config.h
dcf77pi.o: $(hdr)

dcf77pi: $(obj)
	$(CC) -o $@ $(obj) -lm -lncurses

readpin.o: input.h
readpin: readpin.o input.o config.o
	$(CC) -o $@ readpin.o input.o config.o -lrt -lm

clean:
	rm dcf77pi $(obj)
	rm readpin readpin.o

install: dcf77pi readpin
	install dcf77pi readpin $(FULLPREFIX)/bin
	mkdir -p $(FULLPREFIX)/$(ETCDIR)
	install etc/dcf77pi/config.txt $(FULLPREFIX)/$(ETCDIR)/config.txt.sample

lint:
	lint -aabcehrsxgz -D__linux__ -DETCDIR=\"$(ETCDIR)\" $(src) readpin.c
	lint -aabcehrsxgz -D__FreeBSD__ -D__FreeBSD_version=900022 -DETCDIR=\"$(ETCDIR)\" $(src) readpin.c
