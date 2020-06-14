// Copyright 2014-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "mainloop_live.h"

#include "bits1to14.h"
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"
#include "setclock.h"

#include <string.h>
#include <time.h>

static void
check_handle_new_minute(
    struct GB_result bit,
    struct ML_result *mlr,
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
    void (*display_time)(struct DT_result, struct tm),
    struct ML_result (*process_setclock_result)(struct ML_result, int))
{
	bool have_result = false;

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

		if (mlr->settime) {
			have_result = true;
			if (setclock_ok(*init_min, dt, bit)) {
				mlr->settime_result = setclock(*curtime);
			} else {
				mlr->settime_result = esc_unsafe;
			}
		}
		if (bit.marker == emark_minute || bit.marker == emark_late) {
			reset_acc_minlen();
		}
		if (*init_min > 0) {
			(*init_min)--;
		}
	}
	if (have_result) {
		*mlr = process_setclock_result(*mlr, bitpos);
	}
}

void
mainloop_live(
    char *logfilename,
    void (*display_bit)(struct GB_result, int),
    void (*display_long_minute)(void),
    void (*display_minute)(int),
    void (*display_new_second)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(struct DT_result, struct tm),
    void (*display_thirdparty_buffer)(const unsigned[]),
    struct ML_result (*process_setclock_result)(struct ML_result, int),
    struct ML_result (*process_input)(struct ML_result, int),
    struct ML_result (*post_process_input)(struct ML_result, int))
{
	int minlen = 0;
	int bitpos = 0;
	unsigned init_min = 2;
	struct tm curtime;
	struct ML_result mlr;
	bool was_toolong = false;

	(void)memset(&curtime, 0, sizeof(curtime));
	(void)memset(&mlr, 0, sizeof(mlr));
	mlr.logfilename = logfilename;

	for (;;) {
		struct GB_result bit;

		bit = get_bit_live();
		mlr = process_input(mlr, bitpos);
		if (mlr.quit) {
			break;
		}

		bitpos = get_bitpos();
		mlr = post_process_input(mlr, bitpos);
		if (!mlr.quit) {
			display_bit(bit, bitpos);
		}

		if (init_min < 2) {
			fill_thirdparty_buffer(curtime.tm_min, bitpos, bit);
		}

		bit = next_bit();
		if (minlen == -1) {
			check_handle_new_minute(bit, &mlr, bitpos, &curtime,
			    minlen, was_toolong, &init_min, display_minute,
			    display_thirdparty_buffer, display_alarm,
			    display_unknown, display_weather, display_time,
			    process_setclock_result);
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
		display_new_second();

		check_handle_new_minute(bit, &mlr, bitpos, &curtime, minlen,
		    was_toolong, &init_min, display_minute,
		    display_thirdparty_buffer, display_alarm, display_unknown,
		    display_weather, display_time, process_setclock_result);
		was_toolong = false;
		if (mlr.quit) {
			break;
		}
	}
	cleanup();
}
