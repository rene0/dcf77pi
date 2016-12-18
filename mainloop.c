/*
Copyright (c) 2014, 2016 René Ladan. All rights reserved.

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
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"
#include "setclock.h"

#include <string.h>
#include <time.h>

static int mainloop_result;

void
mainloop(char *logfilename,
    const struct GB_result * const (*get_bit)(void),
    void (*display_bit)(const struct GB_result * const, unsigned),
    void (*display_long_minute)(void),
    void (*display_minute)(unsigned),
    void (*display_new_second)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(const struct DT_result * const, struct tm),
    void (*display_thirdparty_buffer)(const unsigned * const),
    void (*show_mainloop_result)(struct GB_result * const, unsigned),
    void (*process_input)(struct GB_result * const, unsigned,
	const char * const, bool * const, bool * const),
    void (*post_process_input)(char **, bool * const, struct GB_result * const,
	unsigned))
{
	unsigned minlen = 0;
	unsigned bitpos = 0;
	unsigned init_min = 2;
	struct tm curtime;
	bool settime = false;
	bool change_logfile = false;
	bool have_result = false;

	init_time();
	(void)memset(&curtime, 0, sizeof(curtime));

	for (;;) {
		const struct DT_result *dt;
		struct GB_result *bit;

		bit = get_bit();
		if (process_input != NULL) {
			process_input(bit, bitpos, logfilename, &settime,
			    &change_logfile);
			if (bit->done)
				break;
		}

		bitpos = get_bitpos();
		if (post_process_input != NULL)
			post_process_input(&logfilename, &change_logfile, bit,
			    bitpos);
		if (bit->skip == eskip_none && !bit->done)
			display_bit(bit, bitpos);

		if (init_min < 2)
			fill_thirdparty_buffer(curtime.tm_min, bitpos, bit);

		bit = next_bit();
		if (bit->marker == emark_minute)
			minlen = bitpos + 1;
			/* handle the missing bit due to the minute marker */
		if (bit->marker == emark_toolong || bit->marker == emark_late) {
			minlen = 0xff;
			/*
			 * leave acc_minlen alone,
			 * any minute marker already processed
			 */
			display_long_minute();
		}
		if (display_new_second != NULL)
			display_new_second();

		if (bit->marker == emark_minute || bit->marker == emark_late) {
			display_minute(minlen);
			dt = decode_time(init_min, minlen, get_acc_minlen(),
			    get_buffer(), &curtime);

			if (curtime.tm_min % 3 == 0 && init_min == 0) {
				const unsigned *tpbuf;

				tpbuf = get_thirdparty_buffer();
				display_thirdparty_buffer(tpbuf);
				switch (get_thirdparty_type()) {
				case eTP_alarm:
					{
						struct alm civwarn;

						decode_alarm(tpbuf, &civwarn);
						display_alarm(civwarn);
					}
					break;
				case eTP_unknown:
					display_unknown();
					break;
				case eTP_weather:
					display_weather();
					break;
				}
			}
			display_time(dt, curtime);

			if (settime) {
				have_result = true;
				if (setclock_ok(init_min, dt, bit))
					mainloop_result = setclock(curtime);
				else
					mainloop_result = -3;
			}
			if (bit->marker == emark_minute ||
			    bit->marker == emark_late)
				reset_acc_minlen();
			if (init_min > 0)
				init_min--;
		}
		if (have_result) {
			if (show_mainloop_result != NULL)
				show_mainloop_result(bit, bitpos);
			have_result = false;
		}
		if (bit->done)
			break;
	}
	cleanup();
}

int
get_mainloop_result(void)
{
	return mainloop_result;
}
