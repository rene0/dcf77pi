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

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#include <ncurses.h>

#include "input.h"
#include "decode_time.h"
#include "decode_alarm.h"
#include "config.h"

WINDOW *main_win;

void
curses_cleanup(char *reason)
{
	cleanup();
	if (main_win != NULL)
		delwin(main_win);
	if (input_win != NULL)
		delwin(input_win);
	if (alarm_win != NULL)
		delwin(alarm_win);
	if (decode_win != NULL)
		delwin(decode_win);
	endwin();
	printf("%s", reason);
}

int
main(int argc, char *argv[])
{
	uint8_t indata[40], civbuf[40];
	uint16_t bit;
	struct tm time, oldtime, isotime;
	time_t epochtime;
	struct timeval tv;
	struct timezone tz;
	uint8_t civ1 = 0, civ2 = 0;
	int dt = 0, bitpos, minlen = 0, acc_minlen = 0, init = 1, init2 = 1;
	int res, opt, settime = 0;
	char *infilename, *logfilename;

	infilename = logfilename = NULL;
	while ((opt = getopt(argc, argv, "f:l:S")) != -1) {
		switch (opt) {
		case 'f' :
			infilename = strdup(optarg);
			if (infilename == NULL) {
				perror("infilename");
				return errno;
			}
			break;
		case 'l' :
			logfilename = strdup(optarg);
			if (logfilename == NULL) {
				perror("logfilename");
				return errno;
			}
			break;
		default:
			printf("usage: %s [-f infile] [-l logfile]\n",
			    argv[0]);
			return EX_USAGE;
		}
	}

	res = read_config_file(ETCDIR"/config.txt");
	if (res != 0) {
		/* non-existent file? */
		cleanup();
		return res;
	}
	res = set_mode(infilename, logfilename);
	if (res != 0) {
		/* something went wrong */
		cleanup();
		return res;
	}
	init_time();

	/* no weird values please */
	bzero(indata, sizeof(indata));
	bzero(&time, sizeof(time));

	if (infilename == NULL) {
		main_win = NULL;
		alarm_win = NULL;
		decode_win = NULL;
		input_win = NULL;
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
		noecho();
		nonl();
		cbreak();
		nodelay(stdscr, TRUE);
		curs_set(0);
		refresh(); /* prevent clearing windows upon getch() / refresh() */

		/* allocate windows */
		main_win = newwin(1, 80, 24, 0);
		if (main_win == NULL) {
			curses_cleanup("Creating main_win failed.\n");
			return 0;
		}
		alarm_win = newwin(2, 80, 4, 0);
		if (alarm_win == NULL) {
			curses_cleanup("Creating alarm_win failed.\n");
			return 0;
		}
		input_win = newwin(4, 80, 7, 0);
		if (input_win == NULL) {
			curses_cleanup("Creating input_win failed.\n");
			return 0;
		}
		decode_win = newwin(3, 80, 0, 0);
		if (decode_win == NULL) {
			curses_cleanup("Creating decode_win failed.\n");
			return 0;
		}
		/* draw initial screen */
		mvwprintw(main_win, 0, 0, "[S] -> toggle time sync   [Q] -> quit");
		mvwchgat(main_win, 0, 1, 1, A_NORMAL, 4, NULL); /* [S] */
		mvwchgat(main_win, 0, 27, 1, A_NORMAL, 4, NULL); /* [Q] */
		wrefresh(main_win);
		mvwprintw(alarm_win, 0, 0, "Civil buffer:");
		mvwprintw(alarm_win, 1, 0, "German civil warning:");
		wrefresh(alarm_win);
		mvwprintw(decode_win, 0, 0, "old");
		mvwprintw(decode_win, 2, 0, "txcall dst leap");
		mvwchgat(decode_win, 2, 0, 15, A_INVIS, 7, NULL);
		wrefresh(decode_win);
		mvwprintw(input_win, 0, 0, "act total       realfreq Hz increment bit");
		wrefresh(input_win);
	}

	for (;;) {
		bit = get_bit();
		if (infilename != NULL && (bit & GETBIT_EOD))
			break;
		if (infilename == NULL && getch() == 'Q')
			break;
		if (infilename == NULL && getch() == 'S')
			settime = 1 - settime;

		if (bit & (GETBIT_RECV | GETBIT_XMIT | GETBIT_RND))
			acc_minlen += 2500;
		else
			acc_minlen += 1000;

		bitpos = get_bitpos();
		if (bit & GETBIT_EOM) {
			/* handle the missing minute marker */
			minlen = bitpos + 1;
			acc_minlen += 1000;
		}
		if (infilename != NULL)
			display_bit();
		else
			display_bit_gui();

		if (init == 0) {
			switch (time.tm_min % 3) {
			case 0:
				/* copy civil warning data */
				if (bitpos > 1 && bitpos < 8)
					indata[bitpos - 2] = bit & GETBIT_ONE;
					/* 2..7 -> 0..5 */
				if (bitpos > 8 && bitpos < 15)
					indata[bitpos - 3] = bit & GETBIT_ONE;
					/* 9..14 -> 6..11 */

				/* copy civil warning flags */
				if (bitpos == 1)
					civ1 = bit & GETBIT_ONE;
				if (bitpos == 8)
					civ2 = bit & GETBIT_ONE;
				break;
			case 1:
				/* copy civil warning data */
				if (bitpos > 0 && bitpos < 15)
					indata[bitpos + 11] = bit & GETBIT_ONE;
					/* 1..14 -> 12..25 */
				break;
			case 2:
				/* copy civil warning data */
				if (bitpos > 0 && bitpos < 15)
					indata[bitpos + 25] = bit & GETBIT_ONE;
					/* 1..14 -> 26..39 */
				if (bitpos == 15)
					memcpy(civbuf, indata, sizeof(civbuf));
					/* take snapshot of civil warning buffer */
				break;
			}
		}

		bit = next_bit(infilename != NULL);
		if (bit & GETBIT_TOOLONG)
			minlen = 61;
			/*
			 * leave acc_minlen alone,
			 * any missing marker already processed
			 */

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			if (infilename != NULL)
				printf(" (%d) %d\n", acc_minlen, minlen);
			else {
				int i, xpos;
				uint8_t *buffer = get_buffer();
				for (xpos = 4, i = 0; i < sizeof(buffer); i++, xpos++) {
					if (is_space_bit(i))
						xpos++;
					mvwprintw(decode_win, 0, xpos, "%u", buffer[bitpos]);
				}
				mvwchgat(decode_win, 0, 0, 80, A_NORMAL, COLOR_PAIR(7), NULL);
				mvwprintw(decode_win, 1, 28, "(%d)", acc_minlen);
				wrefresh(decode_win);
			}
			if (init == 1 || minlen >= 59)
				memcpy((void *)&oldtime, (const void *)&time,
				    sizeof(struct tm));
			dt = decode_time(init, init2, minlen, get_buffer(),
			    &time, &acc_minlen, dt);

			if (time.tm_min % 3 == 0) {
				if (infilename == NULL)
					show_civbuf(civbuf);
				if (civ1 == 1 && civ2 == 1)
					display_alarm(civbuf, infilename != NULL);
				if (civ1 != civ2)
					display_alarm_error(infilename != NULL);
				if (civ1 == 0 && civ2 == 0 && infilename == NULL)
					clear_alarm();
			}

			if (infilename != NULL)
				display_time(dt, time);
			else
				display_time_gui(dt, time);

			if (settime == 1 && init == 0 && init2 == 0 &&
			    ((dt & ~(DT_XMIT | DT_CHDST | DT_LEAP)) == 0) &&
			    ((bit & ~(GETBIT_ONE | GETBIT_EOM)) == 0)) {
				memcpy((void *)&isotime, (const void *)&time,
				    sizeof(struct tm));
				isotime.tm_year -= 1900;
				isotime.tm_mon--;
				isotime.tm_wday %= 7;
				isotime.tm_sec = 0;
				epochtime = mktime(&isotime);
				if (epochtime == -1)
					printf("mktime() failed!\n");
				else {
					tv.tv_sec = epochtime;
					tv.tv_usec = 50000;
					/* adjust for bit reception algorithm */
					printf("Setting time (%lld , %lld)\n",
					    (long long int)tv.tv_sec,
					    (long long int)tv.tv_usec);
					tz.tz_minuteswest = -60;
					tz.tz_dsttime = isotime.tm_isdst;
					if (settimeofday(&tv, &tz) == -1)
						perror("settimeofday");
				}
			}
			if (init == 1 || !((dt & DT_LONG) || (dt & DT_SHORT)))
				acc_minlen = 0; /* really a new minute */
			if (init == 0 && init2 == 1)
				init2 = 0;
			if (init == 1)
				init = 0;
		}
	}

	cleanup();
	if (infilename == NULL) {
		endwin();
	}
	return 0;
}
