// Copyright 2014-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "mainloop_live.h"

#include "bits1to14.h"
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"
#include "setclock.h"

#include <signal.h>
#include <string.h>
#include <time.h>

#include <sys/time.h>

static void
reset_interval(struct bitinfo *bit, struct hardware hw)
{
	write_to_logfile('=');
	bit->interval = 1e6 / hw.freq;
	bit->change_interval = true;
}

static void
check_handle_new_minute(
    struct GB_result gbr,
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

	if ((gbr.marker == emark_minute || gbr.marker == emark_late) &&
	    !was_toolong) {
		struct DT_result dt;

		display_minute(minlen);
		dt = decode_time(*init_min, minlen, 0,
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
			if (setclock_ok(*init_min, dt, gbr)) {
				mlr->settime_result = setclock(*curtime);
			} else {
				mlr->settime_result = esc_unsafe;
			}
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
	struct hardware hw;
	struct GB_result gbr;
	bool is_eom;
	bool was_toolong = false;
	bool synced;
	struct itimerval itv;
	sigset_t signalset;
	int sigwait_clr;
	bool newminute;
	int pulse, oldpulse;
	int count;
	int act, pas;
	int second, bump_second;
	struct bitinfo bit;

	(void)memset(&curtime, 0, sizeof(curtime));
	(void)memset(&gbr, 0, sizeof(gbr));
	(void)memset(&mlr, 0, sizeof(mlr));
	mlr.logfilename = logfilename;

	hw = get_hardware_parameters();

	bit.change_interval = false;
	bit.interval = 1e6 / hw.freq;
	/* set up the timer */
	itv.it_interval.tv_sec = itv.it_value.tv_sec = 0;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = bit.interval;
	(void)setitimer(ITIMER_REAL, &itv, NULL);

	/* set up the signal trap for the timer */
	(void)sigemptyset(&signalset);
	(void)sigaddset(&signalset, SIGALRM);
	(void)sigprocmask(SIG_BLOCK, &signalset, NULL);

	pulse = 3; /* nothing yet */
	oldpulse = -1;
	count = 0;
	act = pas = 0;
	second = 0;
	bump_second = 0;
	synced = false;

	for (;;) {
		gbr = set_new_state(gbr);
		is_eom = gbr.marker == emark_minute || gbr.marker == emark_late;
		pulse = get_pulse();
		// perhaps handle pulse == 2 (hw error)

		if (count >= hw.freq) {
			if (act > 0 && pas == 0) {
				bit.interval--;
				bit.change_interval = true;
				count = act;
			} else {
				count = 0;
			}
			if (pas > 0 && pas < 2 * hw.freq) {
				bit.interval++;
				bit.change_interval = true;
			}
			if (act >= 2 * hw.freq || pas >= 2 * hw.freq) {
				// no radio signal
				act = pas = 0;
				bump_second = 2;
			}
		} // count >= hw.freq
		if (pulse == 1) {
			act++;
		} else {
			pas++;
		}
		if (oldpulse == 0 && pulse == 1) {
			// this assumes a clean signal without a software filter
			if (act + pas > 0.8 * hw.freq) {
				/* start of new second */
				bump_second = 1;
				if (!synced) {
					// first scond
					synced = true;
					count = 0;
				}
				if (count > 0 && count < hw.freq / 2) {
					bit.interval++;
					bit.change_interval = true;
				}
				if (count > hw.freq / 2) {
					bit.interval--;
					bit.change_interval = true;
				}
			}
			newminute = false;
			if (pas > 1.5 * hw.freq) {
				newminute = true;
				bump_second = -1;
				second = 0;
			}
			if (act + pas > 0.8 * hw.freq) {
				// reset here instead of above bcause of the
				// pas minute test
				act = pas = 0;
			}
		}
		/*
		 * Prevent algorithm collapse during thunderstorms or
		 * scheduler abuse
		 */
		if (bit.interval < 8e5 / hw.freq || bit.interval > 1.2e6 / hw.freq) {
			reset_interval(&bit, hw);
		}
		/*
		 * Reset the frequency and the EOM flag if two
		 * consecutive EOM markers come in, which means
		 * something is wrong.
		 */
		if (newminute) {
			if (is_eom) {
				if (gbr.marker == emark_minute) {
					gbr.marker = emark_none;
				} else if (gbr.marker == emark_late) {
					gbr.marker = emark_toolong;
				}
				reset_interval(&bit, hw);
			} else {
				if (gbr.marker == emark_none) {
					gbr.marker = emark_minute;
				} else if (gbr.marker == emark_toolong) {
					gbr.marker = emark_late;
				}
			}
		}
		if (bit.change_interval) {
			bit.change_interval = false;
			itv.it_interval.tv_usec = bit.interval;
			(void)setitimer(ITIMER_REAL, &itv, NULL);
		}
		if (bump_second != 0) {
			if (bump_second != -1) {
				second += bump_second;
				if (second > 60) {
					second = 0;
				}
			}
			bump_second = 0;
			mlr = process_input(mlr, bitpos);
			if (mlr.quit) {
				break;
			}
			bitpos = get_bitpos();
			mlr = post_process_input(mlr, bitpos);
			if (mlr.quit) {
				break;
			} else {
				display_bit(gbr, bitpos);
			}
			if (init_min < 2) {
				fill_thirdparty_buffer(curtime.tm_min, bitpos, gbr);
			}
			gbr = next_bit(gbr);
			if (minlen == -1) {
				check_handle_new_minute(gbr, &mlr, bitpos, &curtime, minlen,
				    was_toolong, &init_min, display_minute,
				    display_thirdparty_buffer, display_alarm, display_unknown,
				    display_weather, display_time, process_setclock_result);
				was_toolong = true;
			}
			if (gbr.marker == emark_minute) {
				minlen = bitpos + 1;
				/* handle the missing bit due to the minute marker */
			} else if (gbr.marker == emark_toolong ||
			    gbr.marker == emark_late) {
				minlen = -1;
				display_long_minute();
			}
			display_new_second();
			check_handle_new_minute(gbr, &mlr, bitpos, &curtime, minlen,
			    was_toolong, &init_min, display_minute,
			    display_thirdparty_buffer, display_alarm, display_unknown,
			    display_weather, display_time, process_setclock_result);
			was_toolong = false;
		}
		oldpulse = pulse;
		count++;
		(void)sigwait(&signalset, &sigwait_clr);
	}
	cleanup();
}
