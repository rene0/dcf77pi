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

#include "bits1to14.h"
#include "config.h"
#include "decode_time.h"
#include "input.h"
#include "mainloop.h"

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include <ncurses.h>

#define MAXBUF 255

WINDOW *input_win;
WINDOW *decode_win;
WINDOW *tp_win;
WINDOW *main_win;

int8_t old_bitpos = -1; /* timer for statusbar inactive */
int8_t input_mode;      /* normal input (statusbar keys) or string input */
char keybuf[MAXBUF];    /* accumulator for string input */

void
statusbar(int8_t bitpos, const char * const fmt, ...)
{
	va_list ap;

	old_bitpos = bitpos;

	wmove(main_win, 1, 0);
	va_start(ap, fmt);
	vw_printw(main_win, fmt, ap);
	va_end(ap);
	wclrtoeol(main_win);
	wrefresh(main_win);
}

void
draw_keys(void)
{
	mvwprintw(main_win, 1, 0, "[Q] -> quit [L] -> change log file"
	    " [S] -> toggle time sync");
	wclrtoeol(main_win);
	mvwchgat(main_win, 1, 1, 1, A_BOLD, 4, NULL); /* [Q] */
	mvwchgat(main_win, 1, 13, 1, A_BOLD, 4, NULL); /* [L] */
	mvwchgat(main_win, 1, 36, 1, A_BOLD, 4, NULL); /* [S] */
	wrefresh(main_win);
}

void
curses_cleanup(const char * const reason)
{
	if (decode_win != NULL)
		delwin(decode_win);
	if (tp_win != NULL)
		delwin(tp_win);
	if (input_win != NULL)
		delwin(input_win);
	if (main_win != NULL)
		delwin(main_win);
	endwin();
	if (reason != NULL)
		printf("%s", reason);
}

void
display_bit(uint16_t state, uint8_t bitpos)
{
	uint8_t xpos, bp;
	uint32_t acc_minlen;
	const struct bitinfo *bitinf;

	bitinf = get_bitinfo();

	mvwprintw(input_win, 3, 1, "%2u %6u %6u %6u %10.3f %10.3f %10.3f",
	    bitpos, bitinf->tlow, bitinf->tlast0, bitinf->t,
	    bitinf->realfreq / 1e6, bitinf->bit0 / 1e6, bitinf->bit20 / 1e6);
	if (bitinf->freq_reset)
		mvwchgat(input_win, 3, 25, 10, A_BOLD, 3, NULL);
	else
		mvwchgat(input_win, 3, 25, 10, A_NORMAL, 7, NULL);
	if (bitinf->bitlen_reset)
		mvwchgat(input_win, 3, 36, 21, A_BOLD, 3, NULL);
	else
		mvwchgat(input_win, 3, 36, 21, A_NORMAL, 7, NULL);

	wattron(input_win, COLOR_PAIR(2));
	if (state & GETBIT_EOM)
		mvwprintw(input_win, 3, 58, "minute   ");
	else if (state == 0 || state == GETBIT_ONE)
		mvwprintw(input_win, 3, 58, "OK       ");
	else
		mvwprintw(input_win, 3, 58, "         ");
	wattroff(input_win, COLOR_PAIR(2));

	wattron(input_win, COLOR_PAIR(1));
	if (state & GETBIT_READ)
		mvwprintw(input_win, 3, 58, "read     ");
	if (state & GETBIT_RECV)
		mvwprintw(input_win, 3, 68, "receive ");
	else if (state & GETBIT_XMIT)
		mvwprintw(input_win, 3, 68, "transmit");
	else if (state & GETBIT_RND)
		mvwprintw(input_win, 3, 68, "random  ");
	else if (state & GETBIT_IO)
		mvwprintw(input_win, 3, 68, "IO      ");
	else {
		wattron(input_win, COLOR_PAIR(2));
		mvwprintw(input_win, 3, 68, "OK      ");
		wattroff(input_win, COLOR_PAIR(2));
	}
	wattroff(input_win, COLOR_PAIR(1));

	for (xpos = bitpos + 4, bp = 0; bp <= bitpos; bp++)
		if (is_space_bit(bp))
			xpos++;

	mvwprintw(input_win, 0, xpos, "%u", get_buffer()[bitpos]);
	if (state & GETBIT_READ)
		mvwchgat(input_win, 0, xpos, 1, A_BOLD, 3, NULL);
	wrefresh(input_win);

	acc_minlen = get_acc_minlen();
	mvwprintw(decode_win, 1, 28, "(%6u)",
	    acc_minlen > 999999 ? 999999 : acc_minlen);
	wrefresh(decode_win);
}

void
display_time(uint32_t dt, struct tm time)
{
	uint16_t cutoff;

	/* color bits depending on the results */
	mvwchgat(decode_win, 0, 4, 1, A_NORMAL, dt & DT_B0 ? 1 : 2, NULL);
	mvwchgat(decode_win, 0, 24, 2, A_NORMAL, dt & DT_DSTERR ? 1 : 2, NULL);
	mvwchgat(decode_win, 0, 29, 1, A_NORMAL, dt & DT_B20 ? 1 : 2, NULL);
	mvwchgat(decode_win, 0, 39, 1, A_NORMAL, dt & DT_MIN ? 1 : 2, NULL);
	mvwchgat(decode_win, 0, 48, 1, A_NORMAL, dt & DT_HOUR ? 1 : 2, NULL);
	mvwchgat(decode_win, 0, 76, 1, A_NORMAL, dt & DT_DATE ? 1 : 2, NULL);
	if (dt & DT_LEAPONE)
		mvwchgat(decode_win, 0, 78, 1, A_NORMAL, 3, NULL);

	/* display date and time */
	mvwprintw(decode_win, 1, 0, "%s %04d-%02d-%02d %s %02d:%02d",
	    time.tm_isdst ? "summer" : "winter", time.tm_year, time.tm_mon,
	    time.tm_mday, get_weekday(time.tm_wday), time.tm_hour, time.tm_min);
	/* display minute cutoff value at end of line (for now at least) */
	cutoff = get_cutoff();
	if (cutoff == 0xffff)
		mvwprintw(decode_win, 1, 74, "?     ");
	else
		mvwprintw(decode_win, 1, 74, "%6.4f", cutoff/10000.0);
	mvwchgat(decode_win, 1, 0, 80, A_NORMAL, 7, NULL);

	/* color date/time string depending on the results */
	if (dt & DT_DSTJUMP)
		mvwchgat(decode_win, 1, 0, 6, A_BOLD, 3, NULL);
	if (dt & DT_YEARJUMP)
		mvwchgat(decode_win, 1, 7, 4, A_BOLD, 3, NULL);
	if (dt & DT_MONTHJUMP)
		mvwchgat(decode_win, 1, 12, 2, A_BOLD, 3, NULL);
	if (dt & DT_MDAYJUMP)
		mvwchgat(decode_win, 1, 15, 2, A_BOLD, 3, NULL);
	if (dt & DT_WDAYJUMP)
		mvwchgat(decode_win, 1, 18, 3, A_BOLD, 3, NULL);
	if (dt & DT_HOURJUMP)
		mvwchgat(decode_win, 1, 22, 2, A_BOLD, 3, NULL);
	if (dt & DT_MINJUMP)
		mvwchgat(decode_win, 1, 25, 2, A_BOLD, 3, NULL);

	/* flip lights depending on the results */
	if ((dt & DT_XMIT) == 0)
		mvwchgat(decode_win, 1, 39, 6, A_NORMAL, 8, NULL);
	if ((dt & ANN_CHDST) == 0)
		mvwchgat(decode_win, 1, 46, 3, A_NORMAL, 8, NULL);
	if (dt & DT_CHDST)
		mvwchgat(decode_win, 1, 46, 3, A_NORMAL, 2, NULL);
	else if (dt & DT_CHDSTERR)
		mvwchgat(decode_win, 1, 46, 3, A_BOLD, 3, NULL);
	if ((dt & ANN_LEAP) == 0)
		mvwchgat(decode_win, 1, 50, 4, A_NORMAL, 8, NULL);
	if (dt & DT_LEAP)
		mvwchgat(decode_win, 1, 50, 4, A_NORMAL, 2, NULL);
	else if (dt & DT_LEAPERR)
		mvwchgat(decode_win, 1, 50, 4, A_BOLD, 3, NULL);
	if (dt & DT_LONG) {
		mvwprintw(decode_win, 1, 56, "long ");
		mvwchgat(decode_win, 1, 56, 5, A_NORMAL, 1, NULL);
	}
	else if (dt & DT_SHORT) {
		mvwprintw(decode_win, 1, 56, "short");
		mvwchgat(decode_win, 1, 56, 5, A_NORMAL, 1, NULL);
	}
	else
		mvwchgat(decode_win, 1, 56, 5, A_NORMAL, 8, NULL);

	wrefresh(decode_win);
}

void
display_thirdparty_buffer(const uint8_t *buf)
{
	uint8_t i;

	for (i = 0; i < TPBUFLEN; i++)
		mvwprintw(tp_win, 0, i + 22, "%u", buf[i]);
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

void
process_input(uint16_t * const bit, uint8_t bitpos,
    const char * const logfilename, bool * const settime,
    bool * const change_logfile)
{
	int inkey;
	char dispbuf[80];
	static uint8_t input_count, input_xpos;

	inkey = getch();
	if (input_mode == 0 && inkey != ERR)
		switch (inkey) {
		case 'Q':
			*bit |= GETBIT_EOD; /* quit main loop */
			break;
		case 'L':
			inkey = ERR; /* prevent key repeat */
			mvwprintw(main_win, 0, 0, "Current log (.): %s",
			    (logfilename && strlen(logfilename) > 0) ?
			    logfilename : "(none)");
			mvwprintw(main_win, 1, 0, "Log file (empty for none):");
			wclrtoeol(main_win);
			wrefresh(main_win);
			input_mode = 1;
			input_count = 0;
			input_xpos = 26;
			*change_logfile = true;
			break;
		case 'S':
			*settime = ! *settime;
			statusbar(bitpos, "Time synchronization %s",
			    *settime ? "on" : "off");
			break;
		}

	while (input_mode == 1 && inkey != ERR) {
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
			wrefresh(main_win);
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
			} else
				mvwprintw(main_win, 1, input_xpos, "%c", inkey);
			wrefresh(main_win);
		}
		inkey = getch();
	}
}

void
post_process_input(char **logfilename, bool * const change_logfile,
    uint16_t * const bit, uint8_t bitpos)
{
	int res;
	char *old_logfilename;

	if (old_bitpos != -1 && (bitpos % 60 == (old_bitpos + 2) % 60 ||
	    (old_bitpos == 57 && bitpos == 0))) {
		/*
		 * Time for status text passed, cannot use *sleep()
		 * in statusbar() because that pauses reception
		 */
		old_bitpos = -1;
		draw_keys();
	}
	if (input_mode == -1) {
		if (*change_logfile) {
			wmove(main_win, 0, 0);
			wclrtoeol(main_win);
			wrefresh(main_win);

			if (*logfilename == NULL)
				*logfilename = strdup("");
			old_logfilename = strdup(*logfilename);
			free(*logfilename);
			*logfilename = strdup(keybuf);
			if (!strcmp(*logfilename, ".")) {
				free(*logfilename);
				*logfilename = strdup(old_logfilename);
			}

			if (strcmp(old_logfilename, *logfilename)) {
				if (strlen(old_logfilename) > 0 &&
				    close_logfile() != 0) {
					statusbar(bitpos,
					    "Error closing old log file");
					*bit |= GETBIT_EOD; /* error */
				}
				if (strlen(*logfilename) > 0) {
					res = write_new_logfile(*logfilename);
					if (res != 0) {
						statusbar(bitpos,
						    strerror(res));
						*bit |= GETBIT_EOD; /* error */
					}
				}
			}
			free(old_logfilename);
			*change_logfile = false;
		}
		input_mode = 0;
	}
}

void
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
display_minute(uint8_t minlen)
{
	uint8_t bp, xpos;

	/* display bits of previous minute */
	for (xpos = 4, bp = 0; bp < minlen; bp++, xpos++) {
		if (bp > 59)
			break;
		if (is_space_bit(bp))
			xpos++;
		mvwprintw(decode_win, 0, xpos, "%u", get_buffer()[bp]);
	}
	wclrtoeol(decode_win);
	mvwchgat(decode_win, 0, 0, 80, A_NORMAL, 7, NULL);
	wrefresh(decode_win);
}

void
show_mainloop_result(uint16_t * const bit, uint8_t bitpos)
{
	switch (get_mainloop_result()) {
	case -1:
		statusbar(bitpos, "mktime() failed!");
		*bit |= GETBIT_EOD; /* error */
		break;
	case -2:
		statusbar(bitpos, "settimeofday(): %s",
		    strerror(errno));
		*bit |= GETBIT_EOD; /* error */
		break;
	case -3:
		statusbar(bitpos, "Too early to set the time");
		break;
	default:
		statusbar(bitpos, "Time set");
		break;
	}
}

int
main(int argc, char *argv[])
{
	int res;
	char *logfilename;

	res = read_config_file(ETCDIR"/config.txt");
	if (res != 0) {
		/* non-existent file? */
		cleanup();
		return res;
	}
	logfilename = get_config_value("outlogfile");
	if (logfilename != NULL && strlen(logfilename) != 0) {
		res = write_new_logfile(logfilename);
		if (res != 0) {
			perror("fopen (logfile)");
			return res;
		}
	}
	res = set_mode_live();
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
	refresh(); /* prevent clearing windows upon getch() / refresh() */

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
	mvwprintw(decode_win, 1, 39, "txcall dst leap");
	mvwchgat(decode_win, 1, 39, 15, A_NORMAL, 8, NULL);
	wrefresh(decode_win);

	mvwprintw(tp_win, 0, 0, "Third party buffer  :");
	mvwprintw(tp_win, 1, 0, "Third party contents:");
	wrefresh(tp_win);

	mvwprintw(input_win, 0, 0, "new");
	mvwprintw(input_win, 2, 0, "bit    act  last0  total   realfreq"
	    "         b0        b20 state     radio");
	wrefresh(input_win);

	draw_keys();

	mainloop(logfilename, get_bit_live, display_bit,
	    display_long_minute, display_minute, wipe_input, display_alarm,
	    display_unknown, display_weather, display_time,
	    display_thirdparty_buffer, show_mainloop_result, process_input,
	    post_process_input);

	curses_cleanup(NULL);
	if (logfilename != NULL)
		free(logfilename);
	return res;
}
