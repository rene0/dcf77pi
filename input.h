/*
Copyright (c) 2013-2014 René Ladan. All rights reserved.

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

#define GETBIT_ONE	(1 << 0)
#define GETBIT_EOM	(1 << 1)
#define GETBIT_EOD	(1 << 2)
#define GETBIT_READ	(1 << 3)
#define GETBIT_TOOLONG	(1 << 4)
#define GETBIT_IO	(1 << 5)
#define GETBIT_XMIT	(1 << 6)
#define GETBIT_RECV	(1 << 7)
#define GETBIT_RND	(1 << 8)

#include <stdint.h> /* uintX_t */
#include <ncurses.h>

struct hardware {
	unsigned long freq;
	unsigned int pin, active_high;
};

int set_mode_file(char *infilename);
int set_mode_live(void);
struct hardware *get_hardware_parameters(void);
void cleanup(void);
uint8_t get_pulse(void);
/* get_bit_*() stores result in internal buffer */
uint16_t get_bit_file(void);
uint16_t get_bit_live(void);
void display_bit_file(void);
void display_bit_gui(void);
/*
 * Prepare for next bit.
 * Indeed one function here to prevent significant duplication.
 */
uint16_t next_bit(int fromfile);
uint8_t get_bitpos(void);
uint8_t *get_buffer(void);
int is_space_bit(int bit);
void draw_input_window(void);
int write_new_logfile(char *logfile);
int switch_logfile(WINDOW *win, char **logfile);

WINDOW *input_win;

#endif
