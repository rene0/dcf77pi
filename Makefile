.PHONY: all clean install

PREFIX?=.
FULLPREFIX?=$(PREFIX)
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" -g

all: dcf77pi readpin

hdr = input.h decode_time.h decode_alarm.h
src = dcf77pi.c input.c decode_time.c decode_alarm.c
obj = dcf77pi.o input.o decode_time.o decode_alarm.o

input.o: input.h
decode_time.o: decode_time.h
decode_alarm.o: decode_alarm.h
dcf77pi.o: $(hdr)

dcf77pi: $(obj)
	$(CC) -o $@ $(obj)

readpin.o: input.h
readpin: readpin.o input.o
	$(CC) -o $@ readpin.o input.o

clean:
	rm dcf77pi $(obj)
	rm readpin readpin.o

install: dcf77pi readpin
	install dcf77pi readpin $(FULLPREFIX)/bin
	mkdir -p $(FULLPREFIX)/$(ETCDIR)
	install etc/dcf77pi/hardware.txt $(FULLPREFIX)/$(ETCDIR)/hardware.txt.sample

lint:
	lint -aabcehrsxgz -D__linux__ -DETCDIR=\"$(ETCDIR)\" $(src)
	lint -aabcehrsxgz -D__FreeBSD__ -D__FreeBSD_version=900022 -DETCDIR=\"$(ETCDIR)\" $(src)
