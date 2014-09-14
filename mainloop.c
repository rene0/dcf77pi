/*
Copyright (c) 2014 René Ladan. All rights reserved.

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

#include "mainloop.h"

#include "bits1to14.h"
#include "decode_time.h"

#include <string.h>

int
mainloop(char *logfilename,
    uint16_t (*get_bit)(void),
    void (*display_bit)(uint16_t, int),
    void (*print_long_minute)(void),
    void (*print_minute)(unsigned int),
    void (*process_new_minute)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(uint32_t, struct tm),
    void (*print_thirdparty_buffer)(uint8_t *),
    void (*set_time)(int, uint32_t, uint16_t, int, struct tm),
    void (*process_input)(uint16_t *, int, char *, int *, int *),
    void (*post_process_input)(char **, int *, uint16_t *, int))
{
	uint16_t bit;
	uint32_t dt = 0;
	unsigned int minlen = 0;
	int bitpos = 0;
	int init = 3;
	struct tm time;
	struct alm civwarn;
	uint8_t *tpbuf;
	int settime = 0;
	int change_logfile = 0;

	init_time();
	(void)memset(&time, '\0', sizeof(time));
	init_thirdparty();

	for (;;) {
		bit = get_bit();
		if (process_input != NULL)
			process_input(&bit, bitpos, logfilename, &settime,
			    &change_logfile);
		if (bit & GETBIT_EOD)
			break;

		if (bit & (GETBIT_RECV | GETBIT_XMIT | GETBIT_RND))
			add_acc_minlen(2500);
		else
			add_acc_minlen(1000);

		bitpos = get_bitpos();
		if (post_process_input != NULL)
			post_process_input(&logfilename, &change_logfile, &bit,
			    bitpos);
		if (bit & GETBIT_EOM) {
			/* handle the missing bit due to the  minute marker */
			minlen = bitpos + 1;
			add_acc_minlen(1000);
		}
		display_bit(bit, bitpos);

		if (init == 0)
			fill_thirdparty_buffer(time.tm_min, bitpos, bit);

		bit = next_bit();
		if (bit & GETBIT_TOOLONG) {
			minlen = 61;
			/*
			 * leave acc_minlen alone,
			 * any minute marker already processed
			 */
			print_long_minute();
		}
		if (process_new_minute != NULL)
			process_new_minute();

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			print_minute(minlen);
			dt = decode_time(init, minlen, get_buffer(), &time);

			if (time.tm_min % 3 == 0 && init == 0) {
				tpbuf = get_thirdparty_buffer();
				print_thirdparty_buffer(tpbuf);
				switch (get_thirdparty_type()) {
				case TP_ALARM:
					decode_alarm(&civwarn);
					display_alarm(civwarn);
					break;
				case TP_UNKNOWN:
					display_unknown();
					break;
				case TP_WEATHER:
					display_weather();
					break;
				}
			}

			display_time(dt, time);

			if (set_time != NULL && settime == 1)
				set_time(init, dt, bit, bitpos, time);
			if ((init & 1) == 1 ||
			    !((dt & DT_LONG) || (dt & DT_SHORT)))
				reset_acc_minlen(); /* really a new minute */
			if (init == 2)
				init &= ~2;
			if ((init & 1) == 1)
				init &= ~1;
		}
	}

	cleanup();
	return 0;
}
