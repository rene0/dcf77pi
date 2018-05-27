// Copyright 2013-2018 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "bits1to14.h"
#include "calendar.h"
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"
#include "mainloop.h"
#include "setclock.h"

#include <json.h>
#include <curses.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#define MAXBUF 255

static WINDOW *input_win;
static WINDOW *decode_win;
static WINDOW *tp_win;
static WINDOW *main_win;

static int old_bitpos = -1; /* timer for statusbar inactive */
static int input_mode;      /* normal input (statusbar) or string input */
static char keybuf[MAXBUF]; /* accumulator for string input */
static bool show_utc;       /* show time in UTC */

static void
statusbar(int bitpos, const char * const fmt, ...)
{
	va_list ap;

	old_bitpos = bitpos;

	wmove(main_win, 1, 0);
	va_start(ap, fmt);
	vw_printw(main_win, fmt, ap);
	va_end(ap);
	wclrtoeol(main_win);
	wnoutrefresh(main_win);
}

static void
draw_keys(void)
{
	mvwprintw(main_win, 1, 0,
	    "[Q] quit [L] change log file [S] time sync on  "
	    "[u] UTC display on ");
	wclrtoeol(main_win);
	mvwchgat(main_win, 1, 1, 1, A_BOLD, 4, NULL); /* [Q] */
	mvwchgat(main_win, 1, 10, 1, A_BOLD, 4, NULL); /* [L] */
	mvwchgat(main_win, 1, 30, 1, A_BOLD, 4, NULL); /* [S] */
	mvwchgat(main_win, 1, 48, 1, A_BOLD, 4, NULL); /* [u] */
	wnoutrefresh(main_win);
}

static void
curses_cleanup(const char * const reason)
{
	if (decode_win != NULL) {
		delwin(decode_win);
	}
	if (tp_win != NULL) {
		delwin(tp_win);
	}
	if (input_win != NULL) {
		delwin(input_win);
	}
	if (main_win != NULL) {
		delwin(main_win);
	}
	endwin();
	if (reason != NULL) {
		printf("%s", reason);
		cleanup();
	}
}

void
display_bit(struct GB_result bit, int bitpos)
{
	int bp, xpos;
	struct bitinfo bitinf;

	bitinf = get_bitinfo();

	mvwprintw(input_win, 3, 1, "%2u %6u %6u %6u %10.3f %10.3f %10.3f",
	    bitpos, bitinf.tlow, bitinf.tlast0, bitinf.t, bitinf.realfreq / 1e6,
	    bitinf.bit0 / 1e6, bitinf.bit20 / 1e6);
	if (bitinf.freq_reset) {
		mvwchgat(input_win, 3, 25, 10, A_BOLD, 3, NULL);
	} else {
		mvwchgat(input_win, 3, 25, 10, A_NORMAL, 7, NULL);
	}
	if (bitinf.bitlen_reset) {
		mvwchgat(input_win, 3, 36, 21, A_BOLD, 3, NULL);
	} else {
		mvwchgat(input_win, 3, 36, 21, A_NORMAL, 7, NULL);
	}

	wattron(input_win, COLOR_PAIR(2));
	if (bit.marker == emark_minute && bit.bitval != ebv_none) {
		mvwprintw(input_win, 3, 58, "minute   ");
	} else if (bit.marker == emark_none && bit.bitval != ebv_none) {
		mvwprintw(input_win, 3, 58, "OK       ");
	} else {
		mvwprintw(input_win, 3, 58, "?        ");
	}
	wattroff(input_win, COLOR_PAIR(2));

	wattron(input_win, COLOR_PAIR(1));
	if (bit.bitval == ebv_none) {
		mvwprintw(input_win, 3, 58, "read     ");
	}
	if (bit.hwstat == ehw_receive) {
		mvwprintw(input_win, 3, 68, "receive ");
	} else if (bit.hwstat == ehw_transmit) {
		mvwprintw(input_win, 3, 68, "transmit");
	} else if (bit.hwstat == ehw_random) {
		mvwprintw(input_win, 3, 68, "random  ");
	} else if (bit.bad_io) {
		mvwprintw(input_win, 3, 68, "IO      ");
	} else {
		/* bit.hwstat == ehw_ok */
		wattron(input_win, COLOR_PAIR(2));
		mvwprintw(input_win, 3, 68, "OK      ");
		wattroff(input_win, COLOR_PAIR(2));
	}
	wattroff(input_win, COLOR_PAIR(1));

	for (xpos = bitpos + 4, bp = 0; bp <= bitpos; bp++) {
		if (is_space_bit(bp)) {
			xpos++;
		}
	}

	mvwprintw(input_win, 0, xpos, "%u", get_buffer()[bitpos]);
	if (bit.bitval == ebv_none) {
		mvwchgat(input_win, 0, xpos, 1, A_BOLD, 3, NULL);
	}
	wnoutrefresh(input_win);

	mvwprintw(decode_win, 1, 28, "(%10u", get_acc_minlen());
	mvwprintw(decode_win, 1, 46, ")");
	wnoutrefresh(decode_win);
	doupdate();
}

void
display_time(struct DT_result dt, struct tm time)
{
	if (show_utc) {
		time = get_utctime(time);
	}

	/* color bits depending on the results */
	mvwchgat(decode_win, 0, 4, 1, A_NORMAL, dt.bit0_ok ? 2 : 1, NULL);
	mvwchgat(decode_win, 0, 24, 2, A_NORMAL,
	    dt.dst_status == eDST_error ? 1 : 2, NULL);
	mvwchgat(decode_win, 0, 29, 1, A_NORMAL, dt.bit20_ok ? 2 : 1, NULL);
	mvwchgat(decode_win, 0, 39, 1, A_NORMAL,
	    dt.minute_status == eval_parity ? 1 :
	    dt.minute_status == eval_bcd ? 4 : 2, NULL);
	mvwchgat(decode_win, 0, 48, 1, A_NORMAL,
	    dt.hour_status == eval_parity ? 1 :
	    dt.hour_status == eval_bcd ? 4 : 2, NULL);
	mvwchgat(decode_win, 0, 76, 1, A_NORMAL,
	    dt.mday_status == eval_parity ? 1 :
	    (dt.mday_status == eval_bcd || dt.wday_status == eval_bcd ||
	    dt.month_status == eval_bcd || dt.year_status == eval_bcd) ? 4 : 2,
	    NULL);
	if (dt.leapsecond_status == els_one) {
		mvwchgat(decode_win, 0, 78, 1, A_NORMAL, 3, NULL);
	}

	/* display date and time */
	mvwprintw(decode_win, 1, 0, "%s %04d-%02d-%02d %s %02d:%02d",
	    time.tm_isdst == 1 ? "summer" : time.tm_isdst == 0 ? "winter" :
	    time.tm_isdst == -2 ? "UTC   " : "?     ", time.tm_year,
	    time.tm_mon, time.tm_mday, weekday[time.tm_wday], time.tm_hour,
	    time.tm_min);

	mvwchgat(decode_win, 1, 0, 80, A_NORMAL, 7, NULL);

	/* color date/time string value depending on the results */
	if (dt.dst_status == eDST_jump) {
		mvwchgat(decode_win, 1, 0, 6, A_BOLD, 3, NULL);
	}
	if (dt.year_status == eval_jump) {
		mvwchgat(decode_win, 1, 7, 4, A_BOLD, 3, NULL);
	}
	if (dt.month_status == eval_jump) {
		mvwchgat(decode_win, 1, 12, 2, A_BOLD, 3, NULL);
	}
	if (dt.mday_status == eval_jump) {
		mvwchgat(decode_win, 1, 15, 2, A_BOLD, 3, NULL);
	}
	if (dt.wday_status == eval_jump) {
		mvwchgat(decode_win, 1, 18, 3, A_BOLD, 3, NULL);
	}
	if (dt.hour_status == eval_jump) {
		mvwchgat(decode_win, 1, 22, 2, A_BOLD, 3, NULL);
	}
	if (dt.minute_status == eval_jump) {
		mvwchgat(decode_win, 1, 25, 2, A_BOLD, 3, NULL);
	}

	/* flip lights depending on the results */
	if (!dt.transmit_call) {
		mvwchgat(decode_win, 1, 50, 6, A_NORMAL, 8, NULL);
	}
	if (!dt.dst_announce) {
		mvwchgat(decode_win, 1, 57, 3, A_NORMAL, 8, NULL);
	} else if (dt.dst_status == eDST_done) {
		mvwchgat(decode_win, 1, 57, 3, A_NORMAL, 2, NULL);
	}
	if (!dt.leap_announce) {
		mvwchgat(decode_win, 1, 61, 4, A_NORMAL, 8, NULL);
	} else if (dt.leapsecond_status == els_done) {
		mvwchgat(decode_win, 1, 61, 4, A_NORMAL, 2, NULL);
	}
	if (dt.minute_length == emin_long) {
		mvwprintw(decode_win, 1, 67, "long ");
		mvwchgat(decode_win, 1, 67, 5, A_NORMAL, 1, NULL);
	} else if (dt.minute_length == emin_short) {
		mvwprintw(decode_win, 1, 67, "short");
		mvwchgat(decode_win, 1, 67, 5, A_NORMAL, 1, NULL);
	} else {
		mvwchgat(decode_win, 1, 67, 5, A_NORMAL, 8, NULL);
	}

	wrefresh(decode_win);
}

void
display_thirdparty_buffer(const unsigned buf[])
{
	for (int i = 0; i < TPBUFLEN; i++) {
		mvwprintw(tp_win, 0, i + 22, "%u", buf[i]);
	}
	wclrtoeol(tp_win);
	wrefresh(tp_win);
}

/*
 * In live mode, reaching this point means a decoding error as alarm messages
 * were only broadcasted for testing between 2003-10-13 and 2003-12-10.
 */
void
display_alarm(struct alm alarm)
{
	wattron(tp_win, COLOR_PAIR(3) | A_BOLD);
	mvwprintw(tp_win, 1, 22, "German civil warning (decoding error)");
	wattroff(tp_win, COLOR_PAIR(3) | A_BOLD);
	wclrtoeol(tp_win);
	wrefresh(tp_win);
}

void
display_unknown(void)
{
	wattron(tp_win, COLOR_PAIR(1));
	mvwprintw(tp_win, 1, 22, "unknown contents");
	wattroff(tp_win, COLOR_PAIR(1));
	wclrtoeol(tp_win);
	wrefresh(tp_win);
}

void
display_weather(void)
{
	wattron(tp_win, COLOR_PAIR(2));
	mvwprintw(tp_win, 1, 22, "Meteotime weather");
	wattroff(tp_win, COLOR_PAIR(2));
	wclrtoeol(tp_win);
	wrefresh(tp_win);
}

static struct ML_result
process_input(struct ML_result in_ml, int bitpos)
{
	static unsigned input_count;
	static int input_xpos;

	struct ML_result mlr;
	int inkey;

	mlr = in_ml;
	inkey = getch();
	if (input_mode == 0 && inkey != ERR) {
		switch (inkey) {
		case 'Q':
			mlr.quit = true; /* quit main loop */
			break;
		case 'L':
			inkey = ERR; /* prevent key repeat */
			mvwprintw(main_win, 0, 0, "Current log (.): %s",
			    (mlr.logfilename != NULL &&
			    strlen(mlr.logfilename) > 0) ? mlr.logfilename :
			    "(none)");
			mvwprintw(main_win, 1, 0, "Log file (empty for none):");
			wclrtoeol(main_win);
			wnoutrefresh(main_win);
			input_mode = 1;
			input_count = 0;
			input_xpos = 26;
			mlr.change_logfile = true;
			break;
		case 'S':
			mlr.settime = !mlr.settime;
			mvwprintw(main_win, 1, 43, mlr.settime ? "off" : "on ");
			wnoutrefresh(main_win);
			break;
		case 'u':
			show_utc = !show_utc;
			mvwprintw(main_win, 1, 63, show_utc ? "off" : "on ");
			wnoutrefresh(main_win);
			break;
		}
		doupdate();
	}

	while (input_mode == 1 && inkey != ERR) {
		char dispbuf[80];

		if (input_count > 0 &&
		    (inkey == KEY_BACKSPACE || inkey == '\b' || inkey == 127)) {
			input_count--;
			input_xpos--;
			if (input_xpos > 78) {
				/* Shift display line one character to right */
				(void)strncpy(dispbuf, keybuf +
				    (input_xpos - 79), 53);
				dispbuf[54] = '\0';
				mvwprintw(main_win, 1, 27, "%s", dispbuf);
			} else {
				wmove(main_win, 1, input_xpos + 1);
				wclrtoeol(main_win);
			}
			wnoutrefresh(main_win);
		} else if (input_count == MAXBUF - 1 ||
		    (inkey == KEY_ENTER || inkey == '\r' || inkey == '\n')) {
			/* terminate to prevent overflow */
			keybuf[input_count] = '\0';
			draw_keys();
			input_mode = -1;
		} else {
			keybuf[input_count++] = inkey;
			input_xpos++;
			if (input_xpos > 79) {
				/* Shift displayed line one character left */
				(void)strncpy(dispbuf, keybuf +
				    (input_xpos - 79), 53);
				dispbuf[54] = '\0';
				mvwprintw(main_win, 1, 27, "%s", dispbuf);
			} else {
				mvwprintw(main_win, 1, input_xpos, "%c", inkey);
			}
			wnoutrefresh(main_win);
		}
		doupdate();
		inkey = getch();
	}
	return mlr;
}

static struct ML_result
post_process_input(struct ML_result in_ml, int bitpos)
{
	struct ML_result mlr;

	mlr = in_ml;

	if (old_bitpos != -1 && (bitpos % 60 == (old_bitpos + 4) % 60 ||
	    (old_bitpos > 4 && bitpos == 1))) {
		/*
		 * Time for status text passed, cannot use *sleep() in
		 * statusbar() because that pauses reception
		 */
		old_bitpos = -1;
		draw_keys();
	}
	if (input_mode == -1) {
		if (in_ml.change_logfile) {
			char *old_logfilename;

			wmove(main_win, 0, 0);
			wclrtoeol(main_win);
			wnoutrefresh(main_win);

			if (in_ml.logfilename == NULL) {
				mlr.logfilename = strdup("");
			}
			old_logfilename = strdup(mlr.logfilename);
			free(mlr.logfilename);
			mlr.logfilename = strdup(keybuf);
			if (strcmp(mlr.logfilename, ".") == 0) {
				free(mlr.logfilename);
				mlr.logfilename = strdup(old_logfilename);
			}

			if (strcmp(old_logfilename, mlr.logfilename) != 0) {
				if (strlen(old_logfilename) > 0 &&
				    close_logfile() != 0) {
					statusbar(bitpos,
					    "Error closing old log file");
					mlr.quit = true; /* error */
				}
				if (strlen(mlr.logfilename) > 0) {
					int res;

					res = append_logfile(mlr.logfilename);
					if (res != 0) {
						statusbar(bitpos,
						    strerror(res));
						mlr.quit = true; /* error */
					}
				}
			}
			free(old_logfilename);
			mlr.change_logfile = false;
		}
		input_mode = 0;
	}
	doupdate();
	return mlr;
}

static void
wipe_input()
{
	if (get_bitpos() == 0) {
		wmove(input_win, 0, 3);
		wclrtoeol(input_win);
	}
	wrefresh(input_win);
}

void
display_long_minute(void)
{
	wattron(input_win, COLOR_PAIR(1));
	mvwprintw(input_win, 3, 58, "no minute");
	wattroff(input_win, COLOR_PAIR(1));
}

void
display_minute(int minlen)
{
	int bp, cutoff, xpos;

	/* display bits of previous minute */
	for (xpos = 4, bp = 0; bp < minlen; bp++, xpos++) {
		if (bp > 59) {
			break;
		}
		if (is_space_bit(bp)) {
			xpos++;
		}
		mvwprintw(decode_win, 0, xpos, "%u", get_buffer()[bp]);
	}
	wclrtoeol(decode_win);
	mvwchgat(decode_win, 0, 0, 80, A_NORMAL, 7, NULL);

	/* display minute cutoff value */
	cutoff = get_cutoff();
	if (cutoff == -1) {
		mvwprintw(decode_win, 1, 40, "?     ");
		mvwchgat(decode_win, 1, 40, 1, A_BOLD, 3, NULL);
	} else {
		mvwprintw(decode_win, 1, 40, "%6.4f", cutoff / 1e4);
	}

	wrefresh(decode_win);
}

static struct ML_result
process_setclock_result(struct ML_result in_ml, int bitpos)
{
	struct ML_result mlr;

	mlr = in_ml;
	mlr.quit = false;
	switch (mlr.settime_result) {
	case esc_invalid:
		statusbar(bitpos, "mktime() failed");
		mlr.quit = true; /* error */
		break;
	case esc_fail:
		statusbar(bitpos, "clock_settime(): %s", strerror(errno));
		mlr.quit = true; /* error */
		break;
	case esc_unsafe:
		statusbar(bitpos, "Too early or unsafe to set the time");
		break;
	case esc_ok:
		statusbar(bitpos, "Time set");
		break;
	}
	doupdate();
	return mlr;
}

int
main(int argc, char *argv[])
{
	struct json_object *config, *value;
	int res;
	char *logfilename = NULL;

	config = json_object_from_file(ETCDIR "/config.json");
	if (config == NULL) {
		/* non-existent file? */
		cleanup();
		return EX_NOINPUT;
	}
	if (json_object_object_get_ex(config, "outlogfile", &value)) {
		logfilename = (char *)(json_object_get_string(value));
	}
	if (logfilename != NULL && strlen(logfilename) != 0) {
		res = append_logfile(logfilename);
		if (res != 0) {
			perror("fopen (logfile)");
			return res;
		}
	}
	res = set_mode_live(config);
	if (res != 0) {
		/* something went wrong */
		cleanup();
		return res;
	}

	decode_win = NULL;
	tp_win = NULL;
	input_win = NULL;
	main_win = NULL;

	initscr();
	if (has_colors() == FALSE) {
		curses_cleanup("No required color support.\n");
		return 0;
	}

	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK); /* turn on A_BOLD */
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_BLACK); /* A_INVIS does not work? */

	noecho();
	nonl();
	cbreak();
	nodelay(stdscr, TRUE);
	keypad(stdscr, TRUE);
	curs_set(0);
	refresh(); /* prevent clearing windows upon getch() */

	/* allocate windows */
	decode_win = newwin(2, 80, 0, 0);
	if (decode_win == NULL) {
		curses_cleanup("Creating decode_win failed.\n");
		return 0;
	}
	tp_win = newwin(2, 80, 3, 0);
	if (tp_win == NULL) {
		curses_cleanup("Creating tp_win failed.\n");
		return 0;
	}
	input_win = newwin(4, 80, 6, 0);
	if (input_win == NULL) {
		curses_cleanup("Creating input_win failed.\n");
		return 0;
	}
	main_win = newwin(2, 80, 23, 0);
	if (main_win == NULL) {
		curses_cleanup("Creating main_win failed.\n");
		return 0;
	}

	/* draw initial screen */

	mvwprintw(decode_win, 0, 0, "old");
	mvwprintw(decode_win, 1, 50, "txcall dst leap");
	mvwchgat(decode_win, 1, 50, 15, A_NORMAL, 8, NULL);
	wnoutrefresh(decode_win);

	mvwprintw(tp_win, 0, 0, "Third party buffer  :");
	mvwprintw(tp_win, 1, 0, "Third party contents:");
	wnoutrefresh(tp_win);

	mvwprintw(input_win, 0, 0, "new");
	mvwprintw(input_win, 2, 0, "bit    act  last0  total   "
	    "realfreq         b0        b20 state     radio");
	wnoutrefresh(input_win);

	draw_keys();
	doupdate();

	mainloop(logfilename, get_bit_live, display_bit, display_long_minute,
	    display_minute, wipe_input, display_alarm, display_unknown,
	    display_weather, display_time, display_thirdparty_buffer,
	    process_setclock_result, process_input, post_process_input);

	curses_cleanup(NULL);
	if (logfilename != NULL) {
		free(logfilename);
	}
	return res;
}
