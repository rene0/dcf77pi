all: dcf77pi

dcf77pi:
	$(CC) $(CFLAGS) -c input.c -o input.o
	$(CC) $(CFLAGS) -c decode_time.c -o decode_time.o
	$(CC) $(CFLAGS) -c decode_alarm.c -o decode_alarm.o
	$(CC) $(CFLAGS) -c dcf77pi.c -o dcf77pi.o
	$(CC) -o dcf77pi *.o

clean:
	$(RM) *.o dcf77pi
