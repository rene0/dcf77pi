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
		case 'S' :
			settime = 1;
			break;
		default:
			printf("usage: %s [-f infile] [-l logfile] [-S]\n",
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
		initscr();
		if (has_colors() == FALSE) {
			cleanup();
			printf("No required color support.\n");
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
	}

	for (;;) {
		bit = get_bit();
		if (bit & GETBIT_EOD)
			break;
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
		display_bit();
		(void)fflush(stdout);

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

		bit = next_bit();
		if (bit & GETBIT_TOOLONG) {
			printf(" L");
			minlen = 61;
			/*
			 * leave acc_minlen alone,
			 * any missing marker already processed
			 */
		}

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			printf(" (%d) %d\n", acc_minlen, minlen);
			if (init == 1 || minlen >= 59)
				memcpy((void *)&oldtime, (const void *)&time,
				    sizeof(struct tm));
			dt = decode_time(init, init2, minlen, get_buffer(),
			    &time, &acc_minlen, dt);

			if (time.tm_min % 3 == 0) {
				if (civ1 == 1 && civ2 == 1)
					display_alarm(civbuf);
				if (civ1 != civ2)
					printf("Civil warning error\n");
			}

			display_time(dt, time);
			printf("\n");

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
