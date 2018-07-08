# Copyright 2017 René Ladan
# SPDX-License-Identifier: BSD-2-Clause

.PHONY: all clean test

objbin=test_calendar.o test_bits1to14.o
exebin=${objbin:.o=}

all: test
test: $(exebin)
	./test_calendar
	./test_bits1to14

JSON_L?=`pkg-config --libs json-c`
PREFIX?=.
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -D_POSIX_C_SOURCE=200809L -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" \
	-g -std=c99

test_calendar.o: ../calendar.h
	$(CC) -fpic $(CFLAGS) -I.. -c test_calendar.c -o $@
test_calendar: test_calendar.o ../calendar.o
	$(CC) -o $@ test_calendar.o ../calendar.o
test_bits1to14.o: ../bits1to14.h
	$(CC) -fpic $(CFLAGS) -I.. -c test_bits1to14.c -o $@
test_bits1to14: test_bits1to14.o ../bits1to14.o ../input.o
	$(CC) -o $@ test_bits1to14.o ../bits1to14.o ../input.o -lm -lpthread \
	$(JSON_L)

clean:
	rm -f $(objbin) $(exebin)
