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

#include "decode_time.h"

#include "config.h"
#include "input.h"

#include <stdlib.h>
#include <string.h>

uint8_t announce = 0; /* save DST change and leap second announcements */
int olderr = 0; /* save error state to determine if DST change might be valid */
int summermonth;
int wintermonth;
int leapsecmonths[12];
int num_leapsecmonths;
char *wday[8] = {"???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

void
init_time(void)
{
	char *freeptr, *lsm, *mon;
	int i, m;

	summermonth = strtol(get_config_value("summermonth"), NULL, 10);
	if (summermonth < 1 || summermonth > 12)
		summermonth = 0;
	wintermonth = strtol(get_config_value("wintermonth"), NULL, 10);
	if (wintermonth < 1 || wintermonth > 12)
		wintermonth = 0;

	freeptr = lsm = strdup(get_config_value("leapsecmonths"));
	num_leapsecmonths = 0;
	for (i = 0; (mon = strsep(&lsm, ",")) != NULL; i++) {
		m = strtol(mon, NULL, 10);
		if (m >= 1 && m <= 12) {
			leapsecmonths[i] = strtol(mon, NULL, 10);
			num_leapsecmonths++;
		}
	}
	free(freeptr);
}

int
is_leapsecmonth(int month)
{
	int i;

	if (month < 1)
		month += 12;
	for (i = 0; i < num_leapsecmonths; i++)
		if (leapsecmonths[i] == month)
			return 1;
	return 0;
}

uint8_t
getpar(uint8_t *buffer, int start, int stop)
{
	int i;
	uint8_t par = 0;

	for (i = start; i <= stop; i++)
		par += buffer[i];
	return par & 1;
}

uint8_t
getbcd(uint8_t *buffer, int start, int stop)
{
	int i;
	uint8_t val = 0;
	uint8_t mul = 1;

	for (i = start; i <= stop; i++) {
		val += mul * buffer[i];
		mul *= 2;
	}
	return val;
}

/* based on: xx00-02-28 is a Monday if and only if xx00 is a leap year */
int
century_offset(int year, int month, int day, int weekday)
{
	int d, nw, nd;
	int tmp; /* resulting day of year, 02-28 if xx00 is leap */
	int dayinleapyear[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274,
	    305, 335};

	/* substract year days from weekday, including normal leap years */
	weekday = (weekday - year - year / 4 - ((year % 4) > 0)) % 7;
	if (weekday < 1)
		weekday += 7;

	/* weekday 1 is a Monday, assume this year is a leap year */
	/* if leap, we should reach Monday xx00-02-28 */
	d = dayinleapyear[month - 1] + day;
	if (d < 60) { /* at or before 02-28 (day 59) */
		nw = (59 - d) / 7;
		nd = weekday == 1 ? 0 : 8 - weekday;
		tmp = d + (nw * 7) + nd;
	} else { /* after 02-28 (day 59) */
		d -= ((year % 4) > 0); /* no 02-29 for obvious non-leap years */
		nw = (d - 59) / 7;
		nd = weekday - 1;
		tmp = d - (nw * 7) - nd;
	}
	/* if day-in-year is 59, this year (xx00) is leap */
	if (tmp == 59)
		return 0;
	if (tmp == 53 || tmp == 54 || tmp == 60 || tmp == 61)
		return 1;
	if (tmp == 55 || tmp == 56 || tmp == 62 || tmp == 63)
		return 2;
	if (tmp == 57 || tmp == 58 || tmp == 64 || tmp == 65)
		return 3;
	return -1; /* ERROR */
}

int
isleap(struct tm time)
{
	return (time.tm_year % 4 == 0 && time.tm_year % 100 != 0) ||
	    time.tm_year % 400 == 0;
}

int
lastday(struct tm time)
{
	if (time.tm_mon == 4 || time.tm_mon == 6 || time.tm_mon == 9 ||
	    time.tm_mon == 11)
		return 30;
	if (time.tm_mon == 2)
		return 28 + isleap(time);
	return 31;
}

void
add_minute(struct tm *time, int flags)
{
	/* time->tm_isdst indicates the old situation */
	if (++time->tm_min == 60) {
		if ((announce & ANN_CHDST) && time->tm_min == 60 &&
		    get_utchour(*time) == 0 && time->tm_wday == 7 &&
		    time->tm_mday > lastday(*time) - 7) {
			if (time->tm_isdst == 1 && time->tm_mon == wintermonth)
				time->tm_hour--; /* will become non-DST */
			else if (time->tm_isdst == 0 &&
			    time->tm_mon == summermonth)
				time->tm_hour++; /* will become DST */
		}
		time->tm_min = 0;
		if (++time->tm_hour == 24) {
			time->tm_hour = 0;
			if (++time->tm_wday == 8)
				time->tm_wday = 1;
			if (++time->tm_mday > lastday(*time)) {
				time->tm_mday = 1;
				if (++time->tm_mon == 13) {
					time->tm_mon = 1;
					if (++time->tm_year == BASEYEAR + 400)
						time->tm_year = BASEYEAR;
						/* bump! */
				}
			}
		}
	}
}

int
decode_time(int init, int init2, int minlen, uint8_t *buffer, struct tm *time,
    int *acc_minlen, int old_dt)
{
	unsigned int rval = 0, generr = 0, p1 = 0, p2 = 0, p3 = 0, ok = 0;
	unsigned int tmp, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
	int utchour, increase;

	if (minlen < 59)
		rval |= DT_SHORT;
	if (minlen > 60)
		rval |= DT_LONG;

	if (buffer[0] == 1)
		rval |= DT_B0;
	if (buffer[20] == 0)
		rval |= DT_B20;

	if (buffer[17] == buffer[18])
		rval |= DT_DSTERR;

	generr = rval; /* do not decode if set */

	if (buffer[15] == 1)
		rval |= DT_XMIT;

	increase = 0;
	while (init == 0 && *acc_minlen >= 60000) {
		add_minute(time, old_dt);
		*acc_minlen -= 60000;
		increase++;
	}

	p1 = getpar(buffer, 21, 28);
	tmp0 = getbcd(buffer, 21, 24);
	tmp1 = getbcd(buffer, 25, 27);
	if (p1 == 1 || tmp0 > 9 || tmp1 > 5) {
		rval |= DT_MIN;
		p1 = 1;
	}
	if ((init == 1 || increase > 0) && p1 == 0 && generr == 0) {
		tmp = tmp0 + 10 * tmp1;
		if (init2 == 0 && time->tm_min != tmp)
			rval |= DT_MINJUMP;
		time->tm_min = tmp;
	}

	p2 = getpar(buffer, 29, 35);
	tmp0 = getbcd(buffer, 29, 32);
	tmp1 = getbcd(buffer, 33, 34);
	if (p2 == 1 || tmp0 > 9 || tmp1 > 2 || tmp0 + 10 * tmp1 > 23) {
		rval |= DT_HOUR;
		p2 = 1;
	}
	if ((init == 1 || increase > 0) && p2 == 0 && generr == 0) {
		tmp = tmp0 + 10 * tmp1;
		if (init2 == 0 && time->tm_hour != tmp)
			rval |= DT_HOURJUMP;
		time->tm_hour = tmp;
	}

	p3 = getpar(buffer, 36, 58);
	tmp0 = getbcd(buffer, 36, 39);
	tmp1 = getbcd(buffer, 40, 41);
	tmp2 = getbcd(buffer, 42, 44);
	tmp3 = getbcd(buffer, 45, 48);
	tmp4 = getbcd(buffer, 50, 53);
	tmp5 = getbcd(buffer, 54, 57);
	if (p3 == 1 || tmp0 > 9 || tmp0 + 10 * tmp1 == 0 ||
	    tmp0 + 10 * tmp1 > 31 || tmp2 == 0 || tmp3 > 9 ||
	    tmp3 + 10 * buffer[49] == 0 || tmp3 + 10 * buffer[49] > 12 ||
	    tmp4 > 9 || tmp5 > 9) {
		rval |= DT_DATE;
		p3 = 1;
	}
	if ((init == 1 || increase > 0) && p3 == 0 && generr == 0) {
		tmp = tmp0 + 10 * tmp1;
		tmp0 = tmp3 + 10 * buffer[49];
		tmp1 = tmp4 + 10 * tmp5;
		if (init2 == 0 && time->tm_mday != tmp)
			rval |= DT_MDAYJUMP;
		if (init2 == 0 && time->tm_wday != tmp2)
			rval |= DT_WDAYJUMP;
		if (init2 == 0 && time->tm_mon != tmp0)
			rval |= DT_MONTHJUMP;
		time->tm_wday = tmp2;
		time->tm_mon = tmp0;
		tmp3 = century_offset(tmp1, tmp0, tmp, tmp2);
		if (tmp3 == -1) {
			rval |= DT_DATE;
			p3 = 1;
		} else {
			if (init2 == 0 && time->tm_year !=
			    BASEYEAR + 100 * tmp3 + tmp1)
				rval |= DT_YEARJUMP;
			time->tm_year = BASEYEAR + 100 * tmp3 + tmp1;
		}
		if (tmp > lastday(*time)) {
			rval |= DT_DATE;
			p3 = 1;
		} else
			time->tm_mday = tmp;
	}

	ok = !generr && !p1 && !p2 && !p3; /* shorthand */

	utchour = get_utchour(*time);

	/* these flags are saved between invocations: */

	/* h==0 (UTC) because sz->wz -> h==2 and wz->sz -> h==1,
	 * last Sunday of month (reference?) */
	if (buffer[16] == 1 && ok) {
		if ((time->tm_wday == 7 && time->tm_mday > lastday(*time) - 7 &&
		    (time->tm_mon == summermonth ||
		    time->tm_mon == wintermonth)) && ((time->tm_min > 0 &&
		    utchour == 0) || (time->tm_min == 0 &&
		    utchour == 1 + buffer[17] - buffer[18]))) {
			announce |= ANN_CHDST;
			if (time->tm_min == 0)
				utchour = 1; /* time zone just changed */
		} else
			rval |= DT_CHDSTERR;
	}

	/*
	 * h==23, last day of month (UTC) or h==0, first day of next month (UTC)
	 * according to IERS Bulletin C
	 */
	if (buffer[19] == 1 && ok) {
		if (time->tm_mday == 1 && is_leapsecmonth(time->tm_mon - 1) &&
		    ((time->tm_min > 0 && utchour == 23) ||
		    (time->tm_min == 0 && utchour == 0)))
			announce |= ANN_LEAP;
		else
			rval |= DT_LEAPERR;
	}

	/* process possible leap second */
	if ((announce & ANN_LEAP) && (minlen == 59 || minlen == 60) &&
	    time->tm_min == 0 && utchour == 0 &&
	    time->tm_mday == 1 && is_leapsecmonth(time->tm_mon - 1)) {
		announce &= ~ANN_LEAP;
		rval |= DT_LEAP;
		if (minlen == 59) {
			rval |= DT_SHORT;
			ok = 0;
			/* leap second processed, but missing */
		} else {
			if (buffer[59] == 1)
				rval |= DT_LEAPONE;
		}
	}
	if ((minlen == 60) && !(rval & DT_LEAP)) {
		rval |= DT_LONG;
		ok = 0;
		/* leap second not processed, so bad minute */
	}

	if ((rval & DT_DSTERR) == 0 && buffer[17] != time->tm_isdst) {
		/* Time offset change is OK if:
		 * there was an error but not any more (needed if decoding at
		 *   startup is problematic)
		 * initial state (otherwise DST would never be valid)
		 * actually announced and time is Sunday, lastday, 01:00 UTC
		 */
		tmp = (announce & ANN_CHDST) && minlen == 59 &&
		    time->tm_min == 0 && utchour == 1 && time->tm_wday == 7 &&
		    time->tm_mday > lastday(*time) - 7 &&
		    (time->tm_mon == summermonth || time->tm_mon == wintermonth);
		if ((olderr && ok) || init == 1 || tmp) {
			time->tm_isdst = buffer[17]; /* expected change */
			if (tmp) {
				announce &= ~ANN_CHDST;
				rval |= DT_CHDST;
			}
		} else {
			rval |= DT_DSTJUMP; /* sudden change, ignore */
			ok = 0;
		}
	}
	time->tm_gmtoff = time->tm_isdst ? 7200 : 3600;

	if (olderr && ok)
		olderr = 0;
	if (!ok)
		olderr = 1;
	return rval;
}

int
get_utchour(struct tm time)
{
	int utchour;

	utchour = time.tm_hour - 1;
	if (time.tm_isdst)
		utchour--;
	if (utchour < 0)
		utchour += 24;
	return utchour;
}

void
display_time_file(int dt, struct tm time)
{
	printf("%s %04d-%02d-%02d %s %02d:%02d\n",
	    time.tm_isdst ? "summer" : "winter", time.tm_year, time.tm_mon,
	    time.tm_mday, wday[time.tm_wday], time.tm_hour, time.tm_min);
	if (dt & DT_LONG)
		printf("Minute too long\n");
	if (dt & DT_SHORT)
		printf("Minute too short\n");
	if (dt & DT_DSTERR)
		printf("Time offset error\n");
	if (dt & DT_DSTJUMP)
		printf("Time offset jump (ignored)\n");
	if (dt & DT_MIN)
		printf("Minute parity/value error\n");
	if (dt & DT_MINJUMP)
		printf("Minute value jump\n");
	if (dt & DT_HOUR)
		printf("Hour parity/value error\n");
	if (dt & DT_HOURJUMP)
		printf("Hour value jump\n");
	if (dt & DT_DATE)
		printf("Date parity/value error\n");
	if (dt & DT_WDAYJUMP)
		printf("Day-of-week value jump\n");
	if (dt & DT_MDAYJUMP)
		printf("Day-of-month value jump\n");
	if (dt & DT_MONTHJUMP)
		printf("Month value jump\n");
	if (dt & DT_YEARJUMP)
		printf("Year value jump\n");
	if (dt & DT_B0)
		printf("Minute marker error\n");
	if (dt & DT_B20)
		printf("Date/time start marker error\n");
	if (dt & DT_XMIT)
		printf("Transmitter call bit set\n");
	if (announce & ANN_CHDST)
		printf("Time offset change announced\n");
	if (announce & ANN_LEAP)
		printf("Leap second announced\n");
	if (dt & DT_CHDST)
		printf("Time offset changed\n");
	if (dt & DT_LEAP) {
		printf("Leap second processed");
		if (dt & DT_LEAPONE)
			printf(", value is 1 instead of 0");
		printf("\n");
	}
	if (dt & DT_CHDSTERR)
		printf("Spurious time offset change announcement\n");
	if (dt & DT_LEAPERR)
		printf("Spurious leap second announcement\n");
	printf("\n");
}

void
display_time_gui(int dt, struct tm time, uint8_t *buffer, int minlen,
    int acc_minlen)
{
	int i, xpos;

	/* display bits of previous minute */
	for (xpos = 4, i = 0; i < minlen; i++, xpos++) {
		if (i > 59)
			break;
		if (is_space_bit(i))
			xpos++;
		mvwprintw(decode_win, 0, xpos, "%u", buffer[i]);
	}
	wclrtoeol(decode_win);
	mvwchgat(decode_win, 0, 0, 80, A_NORMAL, 7, NULL);
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
	mvwprintw(decode_win, 1, 0, "%s %04d-%02d-%02d %s %02d:%02d (%6d)",
	    time.tm_isdst ? "summer" : "winter", time.tm_year, time.tm_mon,
	    time.tm_mday, wday[time.tm_wday], time.tm_hour, time.tm_min,
	    acc_minlen);
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
	if ((announce && ANN_CHDST) == 0)
		mvwchgat(decode_win, 1, 46, 3, A_NORMAL, 8, NULL);
	if (dt & DT_CHDST)
		mvwchgat(decode_win, 1, 46, 3, A_NORMAL, 2, NULL);
	else if (dt & DT_CHDSTERR)
		mvwchgat(decode_win, 1, 46, 3, A_BOLD, 3, NULL);
	if ((announce & ANN_LEAP) == 0)
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
draw_time_window(void)
{
	mvwprintw(decode_win, 0, 0, "old");
	mvwprintw(decode_win, 1, 39, "txcall dst leap");
	mvwchgat(decode_win, 1, 39, 15, A_NORMAL, 8, NULL);
	wrefresh(decode_win);
}
