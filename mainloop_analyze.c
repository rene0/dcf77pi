// Copyright 2014-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "mainloop_analyze.h"

#include "bits1to14.h"
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"

#include <stdbool.h>
#include <string.h>
#include <time.h>

static void
check_handle_new_minute(
    struct GB_result bit,
    int bitpos,
    struct tm *curtime,
    int minlen,
    bool was_toolong,
    unsigned *init_min,
    void (*display_minute)(int),
    void (*display_thirdparty_buffer)(const unsigned[]),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(struct DT_result, struct tm))
{
	if ((bit.marker == emark_minute || bit.marker == emark_late) &&
	    !was_toolong) {
		struct DT_result dt;

		display_minute(minlen);
		dt = decode_time(*init_min, minlen, get_acc_minlen(),
		    get_buffer(), curtime);

		if (curtime->tm_min % 3 == 0 && *init_min == 0) {
			const unsigned *tpbuf;

			tpbuf = get_thirdparty_buffer();
			display_thirdparty_buffer(tpbuf);
			switch (get_thirdparty_type()) {
			case eTP_alarm:
			{
				struct alm civwarn;

				decode_alarm(tpbuf, &civwarn);
				display_alarm(civwarn);
				break;
			}
			case eTP_unknown:
				display_unknown();
				break;
			case eTP_weather:
				display_weather();
				break;
			}
		}
		display_time(dt, *curtime);

		if (bit.marker == emark_minute || bit.marker == emark_late) {
			reset_acc_minlen();
		}
		if (*init_min > 0) {
			(*init_min)--;
		}
	}
}

void
mainloop_analyze(
    void (*display_bit)(struct GB_result, int),
    void (*display_long_minute)(void),
    void (*display_minute)(int),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(struct DT_result, struct tm),
    void (*display_thirdparty_buffer)(const unsigned[]))
{
	int minlen = 0;
	int bitpos = 0;
	unsigned init_min = 2;
	struct tm curtime;
	bool was_toolong = false;

	(void)memset(&curtime, 0, sizeof(curtime));

	for (;;) {
		struct GB_result bit;

		bit = get_bit_file();

		bitpos = get_bitpos();
		if (!bit.skip) {
			display_bit(bit, bitpos);
		}

		if (init_min < 2) {
			fill_thirdparty_buffer(curtime.tm_min, bitpos, bit);
		}

		bit = next_bit();
		if (minlen == -1) {
			check_handle_new_minute(bit, bitpos, &curtime,
			    minlen, was_toolong, &init_min, display_minute,
			    display_thirdparty_buffer, display_alarm,
			    display_unknown, display_weather, display_time);
			was_toolong = true;
		}

		if (bit.marker == emark_minute) {
			minlen = bitpos + 1;
			/* handle the missing bit due to the minute marker */
		} else if (bit.marker == emark_toolong ||
		    bit.marker == emark_late) {
			minlen = -1;
			/*
			 * leave acc_minlen alone, any minute marker already
			 * processed
			 */
			display_long_minute();
		}

		check_handle_new_minute(bit, bitpos, &curtime, minlen,
		    was_toolong, &init_min, display_minute,
		    display_thirdparty_buffer, display_alarm, display_unknown,
		    display_weather, display_time);
		was_toolong = false;
		if (bit.done) {
			break;
		}
	}
	cleanup();
}
