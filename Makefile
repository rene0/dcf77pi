.PHONY: all clean

all: dcf77pi

hdr = input.h decode_time.h decode_alarm.h
obj = dcf77pi.o input.o decode_time.o decode_alarm.o

input.o: input.h
decode_time.o: decode_time.h
decode_alarm.o: decode_alarm.h
dcf77pi.o: $(hdr)

dcf77pi: $(obj)
	$(CC) -o $@ $(obj)

clean:
	rm dcf77pi $(obj)
