/*
Copyright (c) 2013-2015 René Ladan. All rights reserved.

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

#include <stdlib.h>
#include <string.h>

static uint32_t announce; /* save DST change and leap second announcements */
static uint8_t summermonth;
static uint8_t wintermonth;
static uint8_t leapsecmonths[12];
static uint8_t num_leapsecmonths;

static const char * const weekday[8] =
    {"???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
static const uint16_t dayinleapyear[12] =
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

const char * const
get_weekday(uint8_t wday)
{
	return weekday[wday & 7];
}

void
init_time(void)
{
	char *freeptr, *lsm, *mon;
	uint8_t i, m;

	summermonth = (uint8_t)strtol(get_config_value("summermonth"), NULL, 10);
	if (summermonth < 1 || summermonth > 12)
		summermonth = 0;
	wintermonth = (uint8_t)strtol(get_config_value("wintermonth"), NULL, 10);
	if (wintermonth < 1 || wintermonth > 12)
		wintermonth = 0;

	freeptr = lsm = strdup(get_config_value("leapsecmonths"));
	num_leapsecmonths = 0;
	for (i = 0; (mon = strsep(&lsm, ",")) != NULL; i++) {
		m = (uint8_t)strtol(mon, NULL, 10);
		if (m >= 1 && m <= 12) {
			leapsecmonths[i] = m;
			num_leapsecmonths++;
		}
	}
	free(freeptr);
}

static bool
is_leapsecmonth(uint8_t month)
{
	uint8_t i;

	if (month == 0)
		month = 12;
	for (i = 0; i < num_leapsecmonths; i++)
		if (leapsecmonths[i] == month)
			return true;
	return false;
}

static bool
getpar(const uint8_t * const buffer, uint8_t start, uint8_t stop)
{
	uint8_t i, par = 0;

	for (i = start; i <= stop; i++)
		par += buffer[i];
	return (par & 1) == 0;
}

static uint8_t
getbcd(const uint8_t * const buffer, uint8_t start, uint8_t stop)
{
	uint8_t i, mul = 1, val = 0;

	for (i = start; i <= stop; i++) {
		val += mul * buffer[i];
		mul *= 2;
	}
	return val;
}

/* based on: xx00-02-28 is a Monday if and only if xx00 is a leap year */
static int8_t
century_offset(uint8_t year, uint8_t month, uint8_t day, uint8_t weekday)
{
	uint8_t nw, nd;
	uint8_t tmp; /* resulting day of year, 02-28 if xx00 is leap */
	int8_t wd;
	uint16_t d;

	/* substract year days from weekday, including normal leap years */
	wd = (int8_t)((weekday - year - year / 4 - (((year % 4) > 0) ? 1 : 0)) % 7);
	if (wd < 1)
		wd += 7;

	/* weekday 1 is a Monday, assume this year is a leap year */
	/* if leap, we should reach Monday xx00-02-28 */
	d = dayinleapyear[month - 1] + day;
	if (d < 60) { /* at or before 02-28 (day 59) */
		nw = (59 - d) / 7;
		nd = (uint8_t)((wd == 1 ? 0 : 8) - wd);
		tmp = d + (nw * 7) + nd;
	} else { /* after 02-28 (day 59) */
		if ((year % 4) > 0)
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

static bool
isleap(struct tm time)
{
	return (time.tm_year % 4 == 0 && time.tm_year % 100 != 0) ||
	    time.tm_year % 400 == 0;
}

static uint8_t
lastday(struct tm time)
{
	if (time.tm_mon == 4 || time.tm_mon == 6 || time.tm_mon == 9 ||
	    time.tm_mon == 11)
		return 30;
	if (time.tm_mon == 2)
		return (uint8_t)(28 + (isleap(time) ? 1 : 0));
	return 31;
}

void
add_minute(struct tm * const time, bool checkflag)
{
	/* time->tm_isdst indicates the old situation */
	if (++time->tm_min == 60) {
		/* DST flag transmitted until 00:59:16 UTC */
		if ((((announce & ANN_CHDST) == ANN_CHDST) || !checkflag) &&
		    get_utchour(*time) == 0 && time->tm_wday == 7 &&
		    time->tm_mday > (int)(lastday(*time) - 7)) {
			if (time->tm_isdst == 1 && time->tm_mon == (int)wintermonth)
				time->tm_hour--; /* will become non-DST */
			if (time->tm_isdst == 0 && time->tm_mon == (int)summermonth)
				time->tm_hour++; /* will become DST */
		}
		time->tm_min = 0;
		if (++time->tm_hour == 24) {
			time->tm_hour = 0;
			if (++time->tm_wday == 8)
				time->tm_wday = 1;
			time->tm_mday++;
			if (time->tm_mday > (int)lastday(*time)) {
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

void
substract_minute(struct tm * const time, bool checkflag)
{
	if (--time->tm_min == -1) {
		/* DST flag transmitted until 00:59:16 UTC */
		if ((((announce & ANN_CHDST) == ANN_CHDST) || !checkflag) &&
		    get_utchour(*time) == 1 && time->tm_wday == 7 &&
		    time->tm_mday > (int)(lastday(*time) - 7)) {
			/* logic is backwards here */
			if (time->tm_isdst == 1 && time->tm_mon == (int)wintermonth)
				time->tm_hour++; /* will become DST */
			if (time->tm_isdst == 0 && time->tm_mon == (int)summermonth)
				time->tm_hour--; /* will become non-DST */
		}
		time->tm_min = 59;
		if (--time->tm_hour == -1) {
			time->tm_hour = 23;
			if (--time->tm_wday == 0)
				time->tm_wday = 7;
			if (--time->tm_mday == 0) {
				if (--time->tm_mon == 0) {
					time->tm_mon = 12;
					if (--time->tm_year == BASEYEAR - 1)
						time->tm_year = BASEYEAR + 399;
						/* bump! */
				}
				time->tm_mday = (int)lastday(*time);
			}
		}
	}
}

uint32_t
decode_time(uint8_t init_min, uint8_t minlen, uint32_t acc_minlen,
    const uint8_t * const buffer, struct tm * const time)
{
	struct tm newtime;
	uint32_t rval = 0;
	int16_t increase, i;
	uint8_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, utchour;
	int8_t centofs;
	bool generr, p1, p2, p3, ok;
	static uint32_t acc_minlen_partial, old_acc_minlen;
	static bool olderr, prev_toolong;

	memset(&newtime, 0, sizeof(newtime));
	/* Initially, set time offset to unknown */
	if (init_min == 2)
		time->tm_isdst = -1;
	newtime.tm_isdst = time->tm_isdst; /* save DST value */

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

	generr = (rval != 0); /* do not decode if set */

	if (buffer[15] == 1)
		rval |= DT_XMIT;

	/* See if there are any partial / split minutes to be combined: */
	if (acc_minlen <= 59000) {
		acc_minlen_partial += acc_minlen;
		if (acc_minlen_partial >= 60000) {
			acc_minlen = acc_minlen_partial;
			acc_minlen_partial %= 60000;
		}
	}
	/* Calculate number of minutes to increase time with: */
	if (prev_toolong)
		increase = (int16_t)((acc_minlen - old_acc_minlen) / 60000);
	else
		increase = (int16_t)(acc_minlen / 60000);
	if (acc_minlen >= 60000)
		acc_minlen_partial %= 60000;
	/* Account for complete minutes with a short acc_minlen: */
	if (acc_minlen % 60000 > 59000) {
		increase++;
		acc_minlen_partial %= 60000;
	}

	prev_toolong = (minlen == 0xff);
	old_acc_minlen = acc_minlen - (acc_minlen % 60000);

	/* There is no previous time on the very first (partial) minute: */
	if (init_min < 2) {
		for (i = increase; increase > 0 && i > 0; i--)
			add_minute(time, true);
		for (i = increase; increase < 0 && i < 0; i++)
			substract_minute(time, false);
	}

	p1 = getpar(buffer, 21, 28);
	tmp0 = getbcd(buffer, 21, 24);
	tmp1 = getbcd(buffer, 25, 27);
	if (!p1 || tmp0 > 9 || tmp1 > 5) {
		rval |= DT_MIN;
		p1 = false;
	}
	if ((init_min == 2 || increase != 0) && p1 && !generr) {
		newtime.tm_min = (int)(tmp0 + 10 * tmp1);
		if (init_min == 0 && time->tm_min != newtime.tm_min)
			rval |= DT_MINJUMP;
	}

	p2 = getpar(buffer, 29, 35);
	tmp0 = getbcd(buffer, 29, 32);
	tmp1 = getbcd(buffer, 33, 34);
	if (!p2 || tmp0 > 9 || tmp1 > 2 || tmp0 + 10 * tmp1 > 23) {
		rval |= DT_HOUR;
		p2 = false;
	}
	if ((init_min == 2 || increase != 0) && p2 && !generr) {
		newtime.tm_hour = (int)(tmp0 + 10 * tmp1);
		if (init_min == 0 && time->tm_hour != newtime.tm_hour)
			rval |= DT_HOURJUMP;
	}

	p3 = getpar(buffer, 36, 58);
	tmp0 = getbcd(buffer, 36, 39);
	tmp1 = getbcd(buffer, 40, 41);
	tmp2 = getbcd(buffer, 42, 44);
	tmp3 = getbcd(buffer, 45, 48);
	tmp4 = getbcd(buffer, 50, 53);
	tmp5 = getbcd(buffer, 54, 57);
	if (!p3 || tmp0 > 9 || tmp0 + 10 * tmp1 == 0 ||
	    tmp0 + 10 * tmp1 > 31 || tmp2 == 0 || tmp3 > 9 ||
	    tmp3 + 10 * buffer[49] == 0 || tmp3 + 10 * buffer[49] > 12 ||
	    tmp4 > 9 || tmp5 > 9) {
		rval |= DT_DATE;
		p3 = false;
	}
	if ((init_min == 2 || increase != 0) && p3 && !generr) {
		newtime.tm_mday = (int)(tmp0 + 10 * tmp1);
		newtime.tm_mon = (int)(tmp3 + 10 * buffer[49]);
		newtime.tm_year = (int)(tmp4 + 10 * tmp5);
		newtime.tm_wday = (int)tmp2;
		if (init_min == 0 && time->tm_mday != newtime.tm_mday)
			rval |= DT_MDAYJUMP;
		if (init_min == 0 && time->tm_wday != newtime.tm_wday)
			rval |= DT_WDAYJUMP;
		if (init_min == 0 && time->tm_mon != newtime.tm_mon)
			rval |= DT_MONTHJUMP;
		centofs = century_offset((uint8_t)newtime.tm_year, (uint8_t)newtime.tm_mon,
		    (uint8_t)newtime.tm_mday, (uint8_t)newtime.tm_wday);
		if (centofs == -1) {
			rval |= DT_DATE;
			p3 = false;
		} else {
			if (init_min == 0 && time->tm_year !=
			    (int)(BASEYEAR + 100 * centofs + newtime.tm_year))
				rval |= DT_YEARJUMP;
			newtime.tm_year += BASEYEAR + 100 * centofs;
			if (newtime.tm_mday > (int)lastday(newtime)) {
				rval |= DT_DATE;
				p3 = false;
			}
		}
	}

	ok = !generr && p1 && p2 && p3; /* shorthand */

	utchour = get_utchour(*time);

	/*
	 * h==23, last day of month (UTC) or h==0, first day of next month (UTC)
	 * according to IERS Bulletin C
	 */
	if (buffer[19] == 1 && ok) {
		if (time->tm_mday == 1 && is_leapsecmonth((uint8_t)(time->tm_mon - 1)) &&
		    ((time->tm_min > 0 && utchour == 23) ||
		    (time->tm_min == 0 && utchour == 0)))
			announce |= ANN_LEAP;
		else {
			announce &= ~ANN_LEAP;
			rval |= DT_LEAPERR;
		}
	}

	/* process possible leap second, always reset announcement at hh:00 */
	if (((announce & ANN_LEAP) == ANN_LEAP) && time->tm_min == 0) {
		announce &= ~ANN_LEAP;
		rval |= DT_LEAP;
		if (minlen == 59) {
			/* leap second processed, but missing */
			rval |= DT_SHORT;
			ok = false;
			generr = true;
		} else if (minlen == 60 && buffer[59] == 1)
			rval |= DT_LEAPONE;
	}
	if ((minlen == 60) && ((rval & DT_LEAP) == 0)) {
		/* leap second not processed, so bad minute */
		rval |= DT_LONG;
		ok = false;
		generr = true;
	}

	/* h==0 (UTC) because sz->wz -> h==2 and wz->sz -> h==1,
	 * last Sunday of month (reference?) */
	if (buffer[16] == 1 && ok) {
		if ((time->tm_wday == 7 && time->tm_mday > (int)(lastday(*time) - 7) &&
		    (time->tm_mon == (int)summermonth ||
		    time->tm_mon == (int)wintermonth)) && ((time->tm_min > 0 &&
		    utchour == 0) || (time->tm_min == 0 &&
		    utchour == 1 + buffer[17] - buffer[18])))
			announce |= ANN_CHDST; /* time zone just changed */
		else {
			announce &= ~ANN_CHDST;
			rval |= DT_CHDSTERR;
		}
	}

	if ((int)buffer[17] != time->tm_isdst || (int)buffer[18] == time->tm_isdst) {
		/* Time offset change is OK if:
		 * announced and time is Sunday, lastday, 01:00 UTC
		 * there was an error but not any more (needed if decoding at
		 *   startup is problematic)
		 * initial state (otherwise DST would never be valid)
		 */
		if ((((announce & ANN_CHDST) == ANN_CHDST) && time->tm_min == 0) ||
		    (olderr && ok) ||
		    ((rval & DT_DSTERR) == 0 && time->tm_isdst == -1))
			newtime.tm_isdst = (int)buffer[17]; /* expected change */
		else {
			if ((rval & DT_DSTERR) == 0)
				rval |= DT_DSTJUMP; /* sudden change, ignore */
			ok = false;
		}
	}
	/* check if DST is within expected date range */
	if ((time->tm_mon > (int)summermonth && time->tm_mon < (int)wintermonth) ||
	    (time->tm_mon == (int)summermonth && time->tm_wday < 7 &&
	      (int)(lastday(*time)) - time->tm_mday < 7) ||
	    (time->tm_mon == (int)summermonth && time->tm_wday == 7 &&
	      (int)(lastday(*time)) - time->tm_mday < 7 && utchour > 0) ||
	    (time->tm_mon == (int)wintermonth && time->tm_wday < 7 &&
	      (int)(lastday(*time)) - time->tm_mday >= 7) ||
	    (time->tm_mon == (int)wintermonth && time->tm_wday == 7 &&
	      (int)(lastday(*time)) - time->tm_mday < 7 &&
		(utchour == 23 /* previous day */ || utchour == 0))) {
		/* expect DST */
		if (newtime.tm_isdst == 0 && (announce & ANN_CHDST) == 0 &&
		    utchour < 24) {
			rval |= DT_DSTJUMP; /* sudden change */
			ok = false;
		}
	} else {
		/* expect non-DST */
		if (newtime.tm_isdst == 1 && (announce & ANN_CHDST) == 0 &&
		    utchour < 24) {
			rval |= DT_DSTJUMP; /* sudden change */
			ok = false;
		}
	}
	/* done with DST */
	if (((announce & ANN_CHDST) == ANN_CHDST) && time->tm_min == 0) {
		announce &= ~ANN_CHDST;
		rval |= DT_CHDST;
	}
	newtime.tm_gmtoff = 3600 * (newtime.tm_isdst + 1);

	if (olderr && ok)
		olderr = false;
	if (!generr) {
		if (p1)
			time->tm_min = newtime.tm_min;
		if (p2)
			time->tm_hour = newtime.tm_hour;
		if (p3) {
			time->tm_mday = newtime.tm_mday;
			time->tm_mon = newtime.tm_mon;
			time->tm_year = newtime.tm_year;
			time->tm_wday = newtime.tm_wday;
		}
		if ((rval & DT_DSTJUMP) == 0) {
			time->tm_isdst = newtime.tm_isdst;
			time->tm_gmtoff = newtime.tm_gmtoff;
		}
	}
	if (!ok)
		olderr = true;

	return rval | announce;
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

struct tm
dcftime(struct tm isotime)
{
	struct tm dt;

	memcpy((void *)&dt, (const void*)&isotime, sizeof(isotime));
	/* keep original tm_sec */
	dt.tm_mon++;
	dt.tm_year += 1900;
	if (dt.tm_wday == 0)
		dt.tm_wday = 7;
	dt.tm_yday = (int)(dayinleapyear[isotime.tm_mon] + dt.tm_mday -
	    ((isotime.tm_mon > 1 && !isleap(dt)) ? 1 : 0));
	dt.tm_zone = NULL;

	return dt;
}

struct tm
isotime(struct tm dcftime)
{
	struct tm it;

	memcpy((void *)&it, (const void *)&dcftime, sizeof(dcftime));
	it.tm_sec = 0;
	it.tm_mon--;
	it.tm_year -= 1900;
	if (it.tm_wday == 7)
		it.tm_wday = 0;
	it.tm_yday = (int)(dayinleapyear[it.tm_mon] + it.tm_mday -
	    ((it.tm_mon > 1 && !isleap(dcftime)) ? 1 : 0));
	it.tm_zone = NULL;

	return it;
}
