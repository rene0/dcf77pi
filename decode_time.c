/*
Copyright (c) 2013 René Ladan. All rights reserved.

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
#include "decode_time.h"

uint8_t announce = 0; /* save DST change and leap second announcements */
int olderr = 0; /* save error state to determine if DST change might be valid */

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
isleap(struct tm time)
{
	int d, nw, nd;
	int dayinleapyear[12] = {0, 31, 60, 91, 121, 152, 182, 213, 244, 274,
	    305, 335};

	if (time.tm_year % 4 > 0)
		return 0;
	else if (time.tm_year % 4 == 0 && time.tm_year > 0)
		return 1;
	else {	/* year == 0 */
		/* weekday 1 is a Monday, assume this is a leap year */
		/* if leap, we should reach Monday 02-28 */

		d = dayinleapyear[time.tm_mon - 1] + time.tm_mday;
		if (d < 60) { /* at or before 02-28 (day 59) */
			nw = (59 - d) / 7;
			nd = time.tm_wday == 1 ? 0 : 8 - time.tm_wday;
			return d + (nw * 7) + nd == 59;
		} else { /* after 02-28 (day 59) */
			nw = (d - 59) / 7;
			nd = time.tm_wday == 1 ? 0 : time.tm_wday - 1;
			return d - (nw * 7) - nd == 59;
		}
	}
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
add_day(struct tm *time)
{
	if (++time->tm_wday == 8)
		time->tm_wday = 1;
	if (++time->tm_mday > lastday(*time)) {
		time->tm_mday = 1;
		if (++time->tm_mon == 13) {
			time->tm_mon = 1;
			time->tm_year++;
		}
	}
}

void
add_minute(struct tm *time)
{
	int utchour;

	/* time->tm_isdst indicates the old situation */
	if (++time->tm_min == 60) {
		if (announce & ANN_CHDST) {
			utchour = time->tm_hour - 1;
			if (time->tm_isdst)
				utchour--;
			if (utchour == 0 && time->tm_wday == 7 &&
			    time->tm_mday > lastday(*time) - 7) {
				/* TODO check month */
				if (time->tm_isdst)
					time->tm_hour--; /* will become non-DST */
				else
					time->tm_hour++; /* will become DST */
			}
		}
		time->tm_min = 0;
		if (++time->tm_hour == 24) {
			time->tm_hour = 0;
			add_day(time);
		}
	}
}

int
decode_time(int init2, int minlen, uint8_t *buffer, struct tm *time,
    int increase)
{
	int rval = 0, generr = 0, p1 = 0, p2 = 0, p3 = 0, ok = 0;
	unsigned int tmp, tmp1, tmp2, tmp3, tmp4, tmp5;
	int utchour;

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

	p1 = getpar(buffer, 21, 28);
	tmp = getbcd(buffer, 21, 24);
	tmp1 = getbcd(buffer, 25, 27);
	if (p1 || tmp > 9 || tmp1 > 5) {
		rval |= DT_MIN;
		p1 = 1;
	}
	if (increase) {
		if (p1 || generr)
			time->tm_min = (time->tm_min + 1) % 60;
		else
			time->tm_min = tmp + 10 * tmp1;
	}

	p2 = getpar(buffer, 29, 35);
	tmp = getbcd(buffer, 29, 32);
	tmp1 = getbcd(buffer, 33, 34);
	if (p2 || tmp > 9 || tmp1 > 2 || tmp + 10 * tmp1 > 23) {
		rval |= DT_HOUR;
		p2 = 1;
	}
	if (increase) {
		if (p2 || generr) {
			if (time->tm_min == 0)
				time->tm_hour = (time->tm_hour + 1) % 24;
		} else
			time->tm_hour = tmp + 10 * tmp1;
	}

	p3 = getpar(buffer, 36, 58);
	tmp = getbcd(buffer, 36, 39);
	tmp1 = getbcd(buffer, 40, 41);
	tmp2 = getbcd(buffer, 42, 44);
	tmp3 = getbcd(buffer, 45, 48);
	tmp4 = getbcd(buffer, 50, 53);
	tmp5 = getbcd(buffer, 54, 57);
	if (p3 || tmp > 9 || tmp + 10 * tmp1 == 0 || tmp + 10 * tmp1 > 31 ||
	    tmp2 == 0 || tmp3 > 9 || tmp3 + 10 * buffer[49] == 0 ||
	    tmp3 + 10 * buffer[49] > 12 || tmp4 > 9 || tmp5 > 9) {
		rval |= DT_DATE;
		p3 = 1;
	}
	if (increase) {
		if (p3 || generr) {
			if (time->tm_min == 0 && time->tm_hour == 0)
				add_day(time);
		} else {
			time->tm_mday = tmp + 10 * tmp1;
			time->tm_wday = tmp2;
			time->tm_mon = tmp3 + 10 * buffer[49];
			time->tm_year = tmp4 + 10 * tmp5;
		}
	}

	ok = !generr && !p1 && !p2 && !p3; /* shorthand */

	utchour = time->tm_hour - 1;
	if (time->tm_isdst)
		utchour--;

	/* these flags are saved between invocations: */

	/* h==0 (UTC) because sz->wz -> h==2 and wz->sz -> h==1,
	 * last Sunday of month (reference?) */
	if (buffer[16] == 1 && ok) {
		if (time->tm_min > 0 && utchour == 0 && time->tm_wday == 7 &&
		    time->tm_mday > lastday(*time) - 7)
			announce |= ANN_CHDST; /* TODO check month */
		else
			rval |= DT_CHDSTERR;
	}
	/* h==23 (UTC), last day of month according to IERS Bulletin C */
	if (buffer[19] == 1 && ok) {
		if (time->tm_min > 0 && utchour == 23 &&
		    time->tm_mday == lastday(*time))
			announce |= ANN_LEAP; /* TODO check month */
		else
			rval |= DT_LEAPERR;
	}

	/* process possible leap second */
	if ((announce & ANN_LEAP) && ok && time->tm_min == 0 && utchour == 0 &&
		time->tm_mday == 1) { /* TODO check month-1 */
		announce &= ~ANN_LEAP;
		rval |= DT_LEAP;
		if (minlen == 59)
			rval |= DT_SHORT;
			/* leap second processed, but missing */
		else {
			/* minlen == 60 because !generr */
			if (buffer[59] == 1)
				rval |= DT_LEAPONE;
		}
	}

	if ((minlen == 60) && !(rval & DT_LEAP))
		rval |= DT_LONG; /* leap second not processed, so bad minute */

	if (buffer[17] != time->tm_isdst) {
		/* Time offset change is OK if:
		 * there was an error but not any more (needed if decoding at
		 * startup is problematic)
		 * initial state
		 * actually announced and time is Sunday, lastday, 01:00 UTC
		 */
		tmp = (announce & ANN_CHDST) && ok && time->tm_min == 0 &&
		    utchour == 1 && time->tm_wday == 7 &&
		    time->tm_mday > lastday(*time) - 7;
			/* TODO check month */
		if ((olderr && ok) || init2 || tmp) {
			time->tm_isdst = buffer[17]; /* expected change */
			if (tmp) {
				announce &= ~ANN_CHDST;
				rval |= DT_CHDST;
			}
		} else
			rval |= DT_DSTJUMP; /* sudden change, ignore */
	}
	time->tm_gmtoff = time->tm_isdst ? 7200 : 3600;

	if (olderr && ok)
		olderr = 0;
	if (!ok)
		olderr = 1;
	return rval;
}

void
display_time(int init2, int dt, struct tm oldtime, struct tm time,
    int increase)
{
	char *wday[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

	printf("%s %02d-%02d-%02d %s %02d:%02d\n",
	    time.tm_isdst ? "summer" : "winter", time.tm_year, time.tm_mon,
	    time.tm_mday, time.tm_wday > 0 && time.tm_wday < 8 ?
	    wday[time.tm_wday - 1] : "???", time.tm_hour, time.tm_min);
	if (dt & DT_DSTERR)
		printf("Time offset error\n");
	if (dt & DT_DSTJUMP)
		printf("Time offset jump (ignored)\n");
	if (dt & DT_MIN)
		printf("Minute parity/value error\n");
	if (!init2 && increase && oldtime.tm_min != time.tm_min)
		printf("Minute value jump\n");
	if (dt & DT_HOUR)
		printf("Hour parity/value error\n");
	if (!init2 && increase && oldtime.tm_hour != time.tm_hour)
		printf("Hour value jump\n");
	if (dt & DT_DATE)
		printf("Date parity/value error\n");
	if (!init2 && increase && oldtime.tm_wday != time.tm_wday)
		printf("Day-of-week value jump\n");
	if (!init2 && increase && oldtime.tm_mday != time.tm_mday)
		printf("Day-of-month value jump\n");
	if (!init2 && increase && oldtime.tm_mon != time.tm_mon)
		printf("Month value jump\n");
	if (!init2 && increase && oldtime.tm_year != time.tm_year)
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
			printf(", value is one instead of zero");
		printf("\n");
	}
	if (dt & DT_CHDSTERR)
		printf("Spurious time offset change announcement\n");
	if (dt & DT_LEAPERR)
		printf("Spurious leap second announcement\n");
}
