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

#include "decode_time.h"

#include <string.h>
#include <strings.h>

int
mainloop(struct bitinfo *bi, char *logfilename, uint16_t (*get_bit)(void), void (*display_bit)(uint16_t, int), void (*print_long_minute)(void), void (*print_minute)(int, int), void (*process_new_minute)(void), void (*display_alarm)(struct alm), void (*display_alarm_error)(void), void (*display_alarm_ok)(void), void (*display_time)(uint32_t, struct tm), void (*print_civil_buffer)(uint8_t *), void (*set_time)(int, uint32_t, uint16_t, int, struct tm), void (*process_input)(uint16_t *, int, char *, int *, int *), void (*post_process_input)(char **, int *, uint16_t *, int))
{
	uint16_t bit;
	uint32_t dt = 0;
	int minlen = 0, acc_minlen = 0, old_acc_minlen;
	int bitpos;
	int init = 3;
	struct tm time, oldtime;
	struct alm civwarn;
	uint8_t *civbuf;
	int settime = 0;
	int change_logfile = 0;

	init_time();
	bzero(&time, sizeof(time));
	init_alarm();

	for (;;) {
		bit = get_bit();
		if (process_input != NULL)
			process_input(&bit, bitpos, logfilename, &settime, &change_logfile);
		if (bit & GETBIT_EOD)
			break;

		if (bit & (GETBIT_RECV | GETBIT_XMIT | GETBIT_RND))
			acc_minlen += 2500;
		else
			acc_minlen += 1000;

		bitpos = get_bitpos();
		if (post_process_input != NULL)
			post_process_input(&logfilename, &change_logfile, &bit, bitpos);
		if (bit & GETBIT_EOM) {
			/* handle the missing minute marker */
			minlen = bitpos + 1;
			acc_minlen += 1000;
		}
		display_bit(bit, bitpos);

		if (init == 0)
			fill_civil_buffer(time.tm_min, bitpos, bit);

		bit = next_bit();
		if (bit & GETBIT_TOOLONG) {
			minlen = 61;
			/*
			 * leave acc_minlen alone,
			 * any missing marker already processed
			 */
			print_long_minute();
		}
		if (process_new_minute != NULL)
			process_new_minute();

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			old_acc_minlen = acc_minlen;
			print_minute(acc_minlen, minlen);
			if ((init & 1) == 1 || minlen >= 59)
				memcpy((void *)&oldtime, (const void *)&time,
				    sizeof(time));
			dt = decode_time(init, minlen, get_buffer(),
			    &time, &acc_minlen);

			if (time.tm_min % 3 == 0 && init == 0) {
				civbuf = get_civil_buffer();
				print_civil_buffer(civbuf);
				decode_alarm(&civwarn);
				switch (get_civil_status()) {
				case 3:
					display_alarm(civwarn);
					break;
				case 2:
				case 1:
					display_alarm_error();
					break;
				case 0:
					display_alarm_ok();
					break;
				}
			}

			display_time(dt, time);

			if (set_time != NULL && settime == 1)
				set_time(init, dt, bit, bitpos, time);
			if ((init & 1) == 1 || !((dt & DT_LONG) || (dt & DT_SHORT)))
				acc_minlen = 0; /* really a new minute */
			if (init == 2)
				init &= ~2;
			if ((init & 1) == 1)
				init &= ~1;
		}
	}

	cleanup();
	return 0;
}
