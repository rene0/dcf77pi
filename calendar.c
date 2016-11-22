/*
Copyright (c) 2016 René Ladan. All rights reserved.

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

#include "calendar.h"

const uint16_t dayinleapyear[12] =
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

/* based on: xx00-02-28 is a Monday if and only if xx00 is a leap year */
int8_t
century_offset(struct tm time)
{
	uint8_t nw, nd;
	uint8_t tmp; /* resulting day of year, 02-28 if xx00 is leap */
	int8_t wd;
	uint16_t d;

	/* substract year days from weekday, including normal leap years */
	wd = (int8_t)((time.tm_wday - time.tm_year - time.tm_year / 4 -
	    (((time.tm_year % 4) > 0) ? 1 : 0)) % 7);
	if (wd < 1)
		wd += 7;

	/* weekday 1 is a Monday, assume this year is a leap year */
	/* if leap, we should reach Monday xx00-02-28 */
	d = dayinleapyear[time.tm_mon - 1] + time.tm_mday;
	if (d < 60) { /* at or before 02-28 (day 59) */
		nw = (59 - d) / 7;
		nd = (uint8_t)(wd == 1 ? 0 : (8 - wd));
		tmp = d + (nw * 7) + nd;
	} else { /* after 02-28 (day 59) */
		if ((time.tm_year % 4) > 0)
			d--; /* no 02-29 for obvious non-leap years */
		nw = (d - 59) / 7;
		nd = (uint8_t)(wd - 1);
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

bool
isleap(struct tm time)
{
	return (time.tm_year % 4 == 0 && time.tm_year % 100 != 0) ||
	    time.tm_year % 400 == 0;
}

uint8_t
lastday(struct tm time)
{
	if (time.tm_mon == 4 || time.tm_mon == 6 || time.tm_mon == 9 ||
	    time.tm_mon == 11)
		return 30;
	if (time.tm_mon == 2)
		return (uint8_t)(28 + (isleap(time) ? 1 : 0));
	return 31;
}

uint8_t
get_utchour(struct tm time)
{
	int8_t utchour;

	if (time.tm_isdst == -1)
		return 24;
	utchour = (int8_t)(time.tm_hour - 1 - time.tm_isdst);
	if (utchour < 0)
		utchour += 24;
	return (uint8_t)utchour;
}
