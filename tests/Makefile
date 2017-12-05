# Copyright 2017 René Ladan
# SPDX-License-Identifier: BSD-2-Clause

.PHONY: all clean test

all: test
test: testcentury
	./testcentury

PREFIX?=.
ETCDIR?=etc/dcf77pi
CFLAGS+=-Wall -D_POSIX_C_SOURCE=200809L -DETCDIR=\"$(PREFIX)/$(ETCDIR)\" \
	-g -std=c99
objbin=testcentury.o

testcentury.o: ../calendar.h
testcentury: testcentury.o ../calendar.o
	$(CC) -fpic $(CFLAGS) -I.. -c testcentury.c -o $@
	$(CC) -o $@ testcentury.o ../calendar.o

clean:
	rm -f $(objbin) testcentury
