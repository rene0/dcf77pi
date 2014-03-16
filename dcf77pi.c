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

#include "input.h"
#include "decode_time.h"
#include "decode_alarm.h"
#include "config.h"
#include "setclock.h"
#include "guifuncs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

WINDOW *main_win;
int bitpos, old_bitpos = -1;

void
curses_cleanup(char *reason)
{
	if (decode_win != NULL)
		delwin(decode_win);
	if (alarm_win != NULL)
		delwin(alarm_win);
	if (input_win != NULL)
		delwin(input_win);
	if (main_win != NULL)
		delwin(main_win);
	endwin();
	if (reason != NULL)
		printf("%s", reason);
}

int
main(int argc, char *argv[])
{
	uint8_t indata[40], civbuf[40];
	uint16_t bit;
	struct tm time, oldtime;
	struct alm civwarn;
	uint8_t civ1 = 0, civ2 = 0;
	int dt = 0, minlen = 0, acc_minlen = 0, old_acc_minlen;
	int init = 1, init2 = 1;
	int res, opt, settime = 0;
	char *infilename, *logfilename;
	int inkey;
	char *clockres;

	infilename = logfilename = NULL;
	while ((opt = getopt(argc, argv, "f:l:")) != -1) {
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
	bzero(&civbuf, sizeof(civbuf));

	if (infilename == NULL) {
		decode_win = NULL;
		alarm_win = NULL;
		input_win = NULL;
		main_win = NULL;

		if (init_curses())
			curses_cleanup("No required color support.\n");

		/* allocate windows */
		decode_win = newwin(2, 80, 0, 0);
		if (decode_win == NULL) {
			curses_cleanup("Creating decode_win failed.\n");
			return 0;
		}
		alarm_win = newwin(2, 80, 3, 0);
		if (alarm_win == NULL) {
			curses_cleanup("Creating alarm_win failed.\n");
			return 0;
		}
		input_win = newwin(4, 80, 6, 0);
		if (input_win == NULL) {
			curses_cleanup("Creating input_win failed.\n");
			return 0;
		}
		main_win = newwin(1, 80, 24, 0);
		if (main_win == NULL) {
			curses_cleanup("Creating main_win failed.\n");
			return 0;
		}
		/* draw initial screen */
		draw_time_window();
		draw_alarm_window();
		draw_input_window();
		draw_keys(main_win);
	}

	for (;;) {
		bit = get_bit();
		if (infilename != NULL && (bit & GETBIT_EOD))
			break;
		if (infilename == NULL && (inkey = getch()) != ERR) {
			if (inkey == 'Q')
				break;
			if (inkey == 'S') {
				settime = 1 - settime;
				statusbar(main_win, "Time synchronization %s",
				    settime ?  "on" : "off");
				old_bitpos = bitpos; /* start timer */
			}
			}
		}

		if (bit & (GETBIT_RECV | GETBIT_XMIT | GETBIT_RND))
			acc_minlen += 2500;
		else
			acc_minlen += 1000;

		bitpos = get_bitpos();
		check_timer(main_win, &old_bitpos, bitpos);

		if (bit & GETBIT_EOM) {
			/* handle the missing minute marker */
			minlen = bitpos + 1;
			acc_minlen += 1000;
		}
		if (infilename != NULL)
			display_bit_file();
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

		if (infilename == NULL)
			bit = next_bit_gui();
		else
			bit = next_bit_file();
		if (bit & GETBIT_TOOLONG)
			minlen = 61;
			/*
			 * leave acc_minlen alone,
			 * any missing marker already processed
			 */

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			old_acc_minlen = acc_minlen;
			if (infilename != NULL)
				printf(" (%d) %d\n", acc_minlen, minlen);
			if (init == 1 || minlen >= 59)
				memcpy((void *)&oldtime, (const void *)&time,
				    sizeof(time));
			dt = decode_time(init, init2, minlen, get_buffer(),
			    &time, &acc_minlen, dt);

			if (time.tm_min % 3 == 0 && init == 0) {
				decode_alarm(civbuf, &civwarn);
				if (infilename == NULL)
					show_civbuf_gui(civbuf);
				if (civ1 == 1 && civ2 == 1) {
					if (infilename == NULL)
						display_alarm_gui(civwarn);
					else
						display_alarm_file(civwarn);
				}
				if (civ1 != civ2) {
					if (infilename == NULL)
						display_alarm_error_gui();
					else
						display_alarm_error_file();
				}
				if (civ1 == 0 && civ2 == 0 && infilename == NULL)
					clear_alarm_gui();
			}

			if (infilename != NULL)
				display_time_file(dt, time);
			else
				display_time_gui(dt, time, get_buffer(),
				    minlen, old_acc_minlen);

			if (settime == 1 && init == 0 && init2 == 0 &&
			    ((dt & ~(DT_XMIT | DT_CHDST | DT_LEAP)) == 0) &&
			    ((bit & ~(GETBIT_ONE | GETBIT_EOM)) == 0)) {
				clockres = setclock(&time);
				statusbar(main_win, clockres);
				free(clockres);
				old_bitpos = bitpos; /* start timer */
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
	if (infilename == NULL)
		curses_cleanup(NULL);
	return 0;
}
