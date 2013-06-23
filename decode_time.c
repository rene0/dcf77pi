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
		if (mul == 16)
			mul = 10;
	}
	return val;
}

int
lastday(int year, int month)
{
	if (month == 4 || month == 6 || month == 9 || month == 11)
		return 30;
	if (month == 2) {
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
			return 29;
		else
			return 28;
	}
	return 31;
}

/* returns current century (0-based) */
int
get_century(void)
{
	time_t t;
	struct tm *lt;

	t = time(&t);
	lt = localtime(&t);

	return lt->tm_year / 100 + 19;
}

void
add_day(struct tm *time)
{
	time->tm_wday = (time->tm_wday + 1) % 7;
	if (++time->tm_mday > lastday(time->tm_year + 1900, time->tm_mon + 1)) {
		time->tm_mday = 1;
		if (++time->tm_mon > 11) {
			time->tm_mon = 0;
			time->tm_year++;
		}
	}
}

int
add_minute(struct tm *time, int flags)
{
	/* time->tm_isdst indicates the old situation */
	if (++time->tm_min == 60) {
		if (announce & ANN_CHDST) {
			if (time->tm_isdst)
				time->tm_hour--; /* will become DST */
			else
				time->tm_hour++; /* will become non-DST */
			flags |= DT_CHDST;	
			announce &= ~ANN_CHDST;
		}
		time->tm_min = 0;
		if (++time->tm_hour == 24) {
			time->tm_hour = 0;
			add_day(time);
		}
	}
	return flags;
}

int
decode_time(int init2, int minlen, uint8_t *buffer, struct tm *time)
{
	int rval = 0, generr = 0, p1 = 0, p2 = 0, p3 = 0;
	unsigned int tmp, tmp1, tmp2, tmp3;

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
	tmp = getbcd(buffer, 21, 27);
	if (p1 || tmp > 59) {
		rval |= DT_MIN;
		p1 = 1;
	}
	if (p1 || generr)
		time->tm_min = (time->tm_min + 1) % 60;
	else
		time->tm_min = tmp;

	p2 = getpar(buffer, 29, 35);
	tmp = getbcd(buffer, 29, 34);
	if (p2 || tmp > 23) {
		rval |= DT_HOUR;
		p2 = 1;
	}
	if (p2 || generr) {
		if (time->tm_min == 0)
			time->tm_hour = (time->tm_hour + 1) % 24;
	} else
		time->tm_hour = tmp;

	p3 = getpar(buffer, 36, 58);
	tmp = getbcd(buffer, 36, 41);
	tmp1 = getbcd(buffer, 42, 44);
	tmp2 = getbcd(buffer, 45, 49);
	tmp3 = getbcd(buffer, 50, 57);
	if (p3 || tmp == 0 || tmp > 31 || tmp1 == 0 || tmp2 == 0 || tmp2 > 12 || tmp3 > 99) {
		rval |= DT_DATE;
		p3 = 1;
	}
	if (p3 || generr) {
		if (time->tm_min == 0 && time->tm_hour == 0)
			add_day(time);
	} else {
		time->tm_mday = tmp;
		time->tm_wday = tmp1 % 7;
		time->tm_mon = tmp2 - 1;
		time->tm_year = tmp3 + 100 * get_century() - 1900;
	}

	/* these flags are saved between invocations: */
	if (buffer[16] == 1 && generr == 0) /* sz->wz -> h==2 .. wz->sz -> h==1 */
		announce |= ANN_CHDST;
	if (buffer[19] == 1 && generr == 0) /* h==0 (UTC) */
		announce |= ANN_LEAP;

	if (minlen == 59) {
		if ((announce & ANN_LEAP) && (time->tm_min == 0)) {
			announce &= ~ANN_LEAP;
			rval |= DT_LEAP | DT_SHORT;
			/* leap second processed, but missing */	
		}
	}
	if (minlen == 60) {
		if ((announce & ANN_LEAP) && (time->tm_min == 0)) {
			announce &= ~ANN_LEAP;
			rval |= DT_LEAP;
			/* leap second processed */
			if (buffer[59] == 1)
				rval |= DT_LEAPONE;
		} else
			rval |= DT_LONG;
	}

	if (buffer[17] != time->tm_isdst) {
		/* Time offset change is OK if:
		 * there was an error but not any more
		 * initial state
		 * actually announced and minute = 0
		 */
		if ((olderr && !generr && !p1 && !p2 && !p3) || init2 || ((announce & ANN_CHDST) && time->tm_min == 0)) {
			olderr = 0;
			time->tm_isdst = buffer[17]; /* expected change */
		} else
			rval |= DT_DSTERR; /* sudden change */
	}
	time->tm_gmtoff = time->tm_isdst ? 7200 : 3600;

	if (generr || p1 || p2 || p3)
		olderr = 1;
	return rval;
}

void
display_time(int init2, int dt, struct tm oldtime, struct tm time)
{
	printf("%s %s", time.tm_isdst ? "summer" : "winter", asctime(&time));
	if (dt & DT_DSTERR)
		printf("Time offset error\n");
	if (dt & DT_MIN)
		printf("Minute parity/value error\n");
	if (!init2 && oldtime.tm_min != time.tm_min)
		printf("Minute value jump\n");
	if (dt & DT_HOUR)
		printf("Hour parity/value error\n");
	if (!init2 && oldtime.tm_hour != time.tm_hour)
		printf("Hour value jump\n");
	if (dt & DT_DATE)
		printf("Date parity/value error\n");
	if (!init2 && oldtime.tm_wday != time.tm_wday)
		printf("Day-of-week value jump\n");
	if (!init2 && oldtime.tm_mday != time.tm_mday)
		printf("Day-of-month value jump\n");
	if (!init2 && oldtime.tm_mon != time.tm_mon)
		printf("Month value jump\n");
	if (!init2 && oldtime.tm_year != time.tm_year)
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
}
