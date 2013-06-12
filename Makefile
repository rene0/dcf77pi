.PHONY: all clean

all: dcf77pi readpin

hdr = input.h decode_time.h decode_alarm.h
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
