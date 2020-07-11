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
reset_bitlen(struct bitinfo *bit, struct hardware hw)
{
	write_to_logfile('!');
	bit->bit0 = hw.freq / 10;
	bit->bit20 = hw.freq / 5;
	bit->bitlen_reset = true;
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
		dt = decode_time(*init_min, minlen, 0, buffer, curtime);

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
	int pas;
	int second, bump_second;
	struct bitinfo bit;
	char outch = '?';

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
	bit.act = pas = 0;
	second = 0;
	bump_second = 0;
	synced = false;

	/*
	 * One period is either 1000 ms or 2000 ms long (normal or padding for
	 * last). The active part is either 100 ms ('0') or 200 ms ('1') long.
	 * The maximum allowed values as percentage of the second length are
	 * specified as half the value and the whole value of the lengths of
	 * bit 0 and bit 20 respectively.
	 *
	 * ~A > 1.5 * hw.freq: end-of-minute
	 * ~A > 2 * hw.freq: timeout
	 */

	bit.bitlen_reset = false;
	bit.bit0 = hw.freq / 10;
	bit.bit20 = hw.freq / 5;

	for (;;) {
		gbr = set_new_state(gbr);
		is_eom = gbr.marker == emark_minute || gbr.marker == emark_late;

		pulse = get_pulse();
		if (pulse == 2) {
			gbr.bad_io = true;
			outch = '*';
			continue;
		}

		if (count >= hw.freq) {
			if (bit.act > 0 && pas == 0) {
				bit.interval--;
				bit.change_interval = true;
				count = bit.act;
			} else {
				count = 0;
			}
			if (pas > 0 && pas < 2 * hw.freq) {
				bit.interval++;
				bit.change_interval = true;
			}
			if (bit.act >= 2 * hw.freq || pas >= 2 * hw.freq) {
				// no radio signal
				if (bit.act == 0) {
					gbr.hwstat = ehw_receive;
					outch = 'R';
				} else if (pas == 0) {
					/* This assumes no AGC in the hardware. */
					gbr.hwstat = ehw_transmit;
					outch = 'X';
				} else {
					/* Is this actually possible? */
					gbr.hwstat = ehw_random;
					outch = '@';
					reset_interval(&bit, hw);
				}
				bit.act = pas = 0;
				bump_second = 2;
			}
		} // count >= hw.freq
		if (pulse == 1) {
			bit.act++;
		} else {
			pas++;
		}
		if (bit.signal != NULL) {
			if ((count & 7) == 0) {
				bit.signal[count / 8] = 0;
				/* clear data from previous second */
			}
			bit.signal[count / 8] |= pulse <<
			    (unsigned char)(count & 7);
		}
		if (oldpulse == 0 && pulse == 1) {
			// this assumes a clean signal without a software filter
			if (bit.act + pas > 0.8 * hw.freq) {
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
			if (bit.act + pas > 0.8 * hw.freq) {
				if (hw.freq * bit.act * (newminute ? 2 : 1) <
				    (bit.bit0 + bit.bit20) / 2 * count) {
					gbr.bitval = ebv_0;
					outch = '0';
					buffer[bitpos] = 0;
				} else if (hw.freq * bit.act * (newminute ? 2 : 1) <
				    (bit.bit0 + bit.bit20) * count) {
					gbr.bitval = ebv_1;
					outch = '1';
					buffer[bitpos] = 1;
				} else {
					gbr.bitval = ebv_none;
					outch = '_';
				}
				bit.act = pas = 0;
			}
		}
		/*
		 * Prevent algorithm collapse during thunderstorms or
		 * scheduler abuse
		 */
		if (bit.interval < 8e5 / hw.freq || bit.interval > 1.2e6 / hw.freq) {
			reset_interval(&bit, hw);
		}
		if (gbr.hwstat == ehw_ok && gbr.marker == emark_none) {
			float avg;
			if (bitpos == 0 && gbr.bitval == ebv_0) {
				bit.bit0 += (bit.act - bit.bit0) / 2;
			}
			if (bitpos == 20 && gbr.bitval == ebv_1) {
				bit.bit20 += (bit.act - bit.bit20) / 2;
			}
			/* Force sane values during e.g. a thunderstorm */
			if (bit.bit20 < bit.bit0 * 1.5 ||
			    bit.bit20 > bit.bit0 * 3) {
				reset_bitlen(&bit, hw);
			}
			avg = (bit.bit20 - bit.bit0) / 2;
			if (bit.bit0 + avg < hw.freq / 10 ||
			    bit.bit0 - avg > hw.freq / 10) {
				reset_bitlen(&bit, hw);
			}
			if (bit.bit20 + avg < hw.freq / 5 ||
			    bit.bit20 - avg > hw.freq / 5) {
				reset_bitlen(&bit, hw);
			}
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
		if (bump_second != 0) {
			if (bump_second != -1) {
				second += bump_second;
				if (second > 60) {
					second = 0;
				}
			}
			bump_second = 0;

			write_to_logfile(outch);
			if (gbr.marker == emark_minute ||
			    gbr.marker == emark_late) {
				write_to_logfile('\n');
			}

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
		if (bit.change_interval) {
			bit.change_interval = false;
			itv.it_interval.tv_usec = bit.interval;
			(void)setitimer(ITIMER_REAL, &itv, NULL);
		}
		(void)sigwait(&signalset, &sigwait_clr);
	}
	cleanup();
}