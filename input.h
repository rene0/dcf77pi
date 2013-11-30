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

#define GETBIT_ONE	1
#define GETBIT_EOM	2
#define GETBIT_EOD	4
#define GETBIT_READ	8
#define GETBIT_TOOLONG	16
#define GETBIT_IO	32
#define GETBIT_XMIT	64
#define GETBIT_RECV	128
#define GETBIT_RND	256

#include <stdint.h> /* uintX_t */

struct hardware {
	unsigned long freq, realfreq;
	unsigned int pin, active_high, margin, min_len, max_len;
};

int set_mode(int verbose, char *infilename, char *logfilename);
struct hardware *get_hardware_parameters(void);
void cleanup(void);
uint8_t get_pulse(void);
uint16_t get_bit(void); /* stores result in internal buffer */
void display_bit(void);
uint16_t next_bit(void); /* prepare for next bit */
uint8_t get_bitpos(void);
uint8_t *get_buffer(void);

#endif
