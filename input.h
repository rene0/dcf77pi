/*
Copyright (c) 2013 René Ladan. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#ifndef DCF77PI_INPUT_H
#define DCF77PI_INPUT_H

#ifdef __FreeBSD__
#include <sys/gpio.h>
#elif defined(__linux__)
#error Linux is WIP
#else
#error Unsupported operating system, please send a patch to the author
#endif

#define GETBIT_ONE	1
#define GETBIT_EOM	2
#define GETBIT_EOD	4
#define GETBIT_READ	8
#define GETBIT_TOOLONG	16
#define GETBIT_IO	32

struct hardware {
	unsigned long freq, margin;
	int pin;
};

int set_mode(int live, char *filename);
void cleanup(void);
int get_bit(void); /* stores result in internal buffer */
void display_bit(void);
int next_bit(void); /* prepare for next bit */
int get_bitpos(void);
uint8_t *get_buffer(void);
int read_hardware_parameters(char *filename, struct hardware *_hw);
int init_hardware(int pin_nr);

#endif
