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
#include "input.h"

#include <string.h>

int
mainloop(char *logfilename,
    uint16_t (*get_bit)(void),
    void (*display_bit)(uint16_t, uint8_t),
    void (*display_long_minute)(void),
    void (*display_minute)(uint8_t),
    void (*display_new_second)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(uint32_t, struct tm),
    void (*display_thirdparty_buffer)(const uint8_t * const),
    void (*set_time)(uint8_t, uint32_t, uint16_t * const, uint8_t, struct tm),
    void (*process_input)(uint16_t * const, uint8_t, const char * const,
	bool * const, bool * const),
    void (*post_process_input)(char **, bool * const, uint16_t * const,
	uint8_t))
{
	uint16_t bit;
	uint32_t dt = 0;
	uint8_t minlen = 0;
	uint8_t bitpos = 0;
	uint8_t init_min = 2;
	struct tm curtime;
	struct alm civwarn;
	const uint8_t * tpbuf;
	bool settime = false;
	bool change_logfile = false;

	init_time();
	(void)memset(&curtime, '\0', sizeof(curtime));

	for (;;) {
		bit = get_bit();

		if (process_input != NULL) {
			process_input(&bit, bitpos, logfilename, &settime,
			    &change_logfile);
			if (bit & GETBIT_EOD)
				break;
		}

		bitpos = get_bitpos();
		if (post_process_input != NULL)
			post_process_input(&logfilename, &change_logfile, &bit,
			    bitpos);
		if (!((bit & GETBIT_SKIP) || (bit & GETBIT_EOD)))
			display_bit(bit, bitpos);

		if (init_min < 2)
			fill_thirdparty_buffer(curtime.tm_min, bitpos, bit);

		bit = next_bit();
		if (bit & GETBIT_EOM)
			minlen = bitpos + 1;
			/* handle the missing bit due to the minute marker */
		if (bit & GETBIT_TOOLONG) {
			minlen = 0xff;
			/*
			 * leave acc_minlen alone,
			 * any minute marker already processed
			 */
			display_long_minute();
		}
		if (display_new_second != NULL)
			display_new_second();

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			display_minute(minlen);
			dt = decode_time(init_min, minlen, get_acc_minlen(),
			    get_buffer(), &curtime);

			if (curtime.tm_min % 3 == 0 && init_min == 0) {
				tpbuf = get_thirdparty_buffer();
				display_thirdparty_buffer(tpbuf);
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

			display_time(dt, curtime);

			if (set_time != NULL && settime)
				set_time(init_min, dt, &bit, bitpos, curtime);
			if (bit & GETBIT_EOM)
				reset_acc_minlen();
			if (init_min > 0)
				init_min--;
		}

		if (bit & GETBIT_EOD)
			break;
	}

	cleanup();
	return 0;
}
