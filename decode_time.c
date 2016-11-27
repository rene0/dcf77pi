/*
Copyright (c) 2013-2016 René Ladan. All rights reserved.

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

#include "calendar.h"
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint32_t announce; /* save DST change and leap second announcements */
static uint8_t summermonth;
static uint8_t wintermonth;
static uint8_t leapsecmonths[12];
static uint8_t num_leapsecmonths;

void
init_time(void)
{
	char *freeptr, *lsm, *mon;

	summermonth = (uint8_t)strtol(get_config_value("summermonth"), NULL, 10);
	if (summermonth < 1 || summermonth > 12)
		summermonth = 0;
	wintermonth = (uint8_t)strtol(get_config_value("wintermonth"), NULL, 10);
	if (wintermonth < 1 || wintermonth > 12)
		wintermonth = 0;

	freeptr = lsm = strdup(get_config_value("leapsecmonths"));
	num_leapsecmonths = 0;
	for (uint8_t i = 0; (mon = strsep(&lsm, ",")) != NULL; i++) {
		uint8_t m = (uint8_t)strtol(mon, NULL, 10);
		if (m >= 1 && m <= 12) {
			leapsecmonths[i] = m;
			num_leapsecmonths++;
		}
	}
	free(freeptr);
}

static bool
is_leapsecmonth(struct tm time)
{
	uint8_t i;

	/*
	 * Local time is 1 or 2 hours ahead of UTC, which is what the
	 * configuration file uses, so adjust for that.
	 */
	if (--time.tm_mon == 0)
		time.tm_mon = 12;
	for (i = 0; i < num_leapsecmonths; i++)
		if (leapsecmonths[i] == time.tm_mon)
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

uint32_t
decode_time(uint8_t init_min, uint8_t minlen, uint32_t acc_minlen,
    const uint8_t * const buffer, struct tm * const time)
{
	struct tm newtime;
	uint32_t rval = 0;
	int16_t increase;
	uint8_t tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, utchour;
	bool generr, p1, p2, p3, ok;
	static uint32_t acc_minlen_partial, old_acc_minlen;
	static bool olderr, prev_toolong;

	memset(&newtime, 0, sizeof(newtime));
	/* Initially, set time offset to unknown */
	if (init_min == 2)
		time->tm_isdst = -1;
	newtime.tm_isdst = time->tm_isdst; /* save DST value */

	if (minlen < 59)
		rval |= eDT_Short;
	if (minlen > 60)
		rval |= eDT_Long;

	if (buffer[0] == 1)
		rval |= eDT_Bit0;
	if (buffer[20] == 0)
		rval |= eDT_Bit20;

	if (buffer[17] == buffer[18])
		rval |= eDT_DSTError;

	generr = (rval != 0); /* do not decode if set */

	if (buffer[15] == 1)
		rval |= eDT_Transmit;

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
		for (int16_t i = increase; increase > 0 && i > 0; i--)
			add_minute(time);
		for (int16_t i = increase; increase < 0 && i < 0; i++)
			substract_minute(time);
	}

	p1 = getpar(buffer, 21, 28);
	tmp0 = getbcd(buffer, 21, 24);
	tmp1 = getbcd(buffer, 25, 27);
	if (!p1 || tmp0 > 9 || tmp1 > 5) {
		rval |= eDT_Minute;
		p1 = false;
	}
	if ((init_min == 2 || increase != 0) && p1 && !generr) {
		newtime.tm_min = (int)(tmp0 + 10 * tmp1);
		if (init_min == 0 && time->tm_min != newtime.tm_min)
			rval |= eDT_MinuteJump;
	}

	p2 = getpar(buffer, 29, 35);
	tmp0 = getbcd(buffer, 29, 32);
	tmp1 = getbcd(buffer, 33, 34);
	if (!p2 || tmp0 > 9 || tmp1 > 2 || tmp0 + 10 * tmp1 > 23) {
		rval |= eDT_Hour;
		p2 = false;
	}
	if ((init_min == 2 || increase != 0) && p2 && !generr) {
		newtime.tm_hour = (int)(tmp0 + 10 * tmp1);
		if (init_min == 0 && time->tm_hour != newtime.tm_hour)
			rval |= eDT_HourJump;
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
		rval |= eDT_Date;
		p3 = false;
	}
	if ((init_min == 2 || increase != 0) && p3 && !generr) {
		newtime.tm_mday = (int)(tmp0 + 10 * tmp1);
		newtime.tm_mon = (int)(tmp3 + 10 * buffer[49]);
		newtime.tm_year = (int)(tmp4 + 10 * tmp5);
		newtime.tm_wday = (int)tmp2;
		if (init_min == 0 && time->tm_mday != newtime.tm_mday)
			rval |= eDT_MonthDayJump;
		if (init_min == 0 && time->tm_wday != newtime.tm_wday)
			rval |= eDT_WeekDayJump;
		if (init_min == 0 && time->tm_mon != newtime.tm_mon)
			rval |= eDT_MonthJump;
		int8_t centofs = century_offset(newtime);
		if (centofs == -1) {
			rval |= eDT_Date;
			p3 = false;
		} else {
			if (init_min == 0 && time->tm_year !=
			    (int)(base_year + 100 * centofs + newtime.tm_year))
				rval |= eDT_YearJump;
			newtime.tm_year += base_year + 100 * centofs;
			if (newtime.tm_mday > (int)lastday(newtime)) {
				rval |= eDT_Date;
				p3 = false;
			}
		}
	}

	ok = !generr && p1 && p2 && p3; /* shorthand */

	utchour = get_utchour(*time);

	/*
	 * h==23, last day of month (UTC) or h==0, first day of next month (UTC)
	 * according to IERS Bulletin C
	 * flag still set at 00:00 UTC, prevent eDT_LeapSecondError
	 */
	if (buffer[19] == 1 && ok) {
		if (time->tm_mday == 1 && is_leapsecmonth(*time) &&
		    ((time->tm_min > 0 && utchour == 23) ||
		    (time->tm_min == 0 && utchour == 0)))
			announce |= eDT_AnnounceLeapSecond;
		else {
			announce &= ~eDT_AnnounceLeapSecond;
			rval |= eDT_LeapSecondError;
		}
	}

	/* process possible leap second, always reset announcement at hh:00 */
	if (((announce & eDT_AnnounceLeapSecond) == eDT_AnnounceLeapSecond) && time->tm_min == 0) {
		announce &= ~eDT_AnnounceLeapSecond;
		rval |= eDT_LeapSecond;
		if (minlen == 59) {
			/* leap second processed, but missing */
			rval |= eDT_Short;
			ok = false;
			generr = true;
		} else if (minlen == 60 && buffer[59] == 1)
			rval |= eDT_LeapSecondOne;
	}
	if ((minlen == 60) && ((rval & eDT_LeapSecond) == 0)) {
		/* leap second not processed, so bad minute */
		rval |= eDT_Long;
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
			announce |= eDT_AnnounceChDST; /* time zone just changed */
		else {
			announce &= ~eDT_AnnounceChDST;
			rval |= eDT_ChDSTError;
		}
	}

	if ((int)buffer[17] != time->tm_isdst || (int)buffer[18] == time->tm_isdst) {
		/* Time offset change is OK if:
		 * announced and time is Sunday, lastday, 01:00 UTC
		 * there was an error but not any more (needed if decoding at
		 *   startup is problematic)
		 * initial state (otherwise DST would never be valid)
		 */
		if ((((announce & eDT_AnnounceChDST) == eDT_AnnounceChDST) && time->tm_min == 0) ||
		    (olderr && ok) ||
		    ((rval & eDT_DSTError) == 0 && time->tm_isdst == -1))
			newtime.tm_isdst = (int)buffer[17]; /* expected change */
		else {
			if ((rval & eDT_DSTError) == 0)
				rval |= eDT_DSTJump; /* sudden change, ignore */
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
		(utchour >= 22 /* previous day */ || utchour == 0))) {
		/* expect DST */
		if (newtime.tm_isdst == 0 && (announce & eDT_AnnounceChDST) == 0 &&
		    utchour < 24) {
			rval |= eDT_DSTJump; /* sudden change */
			ok = false;
		}
	} else {
		/* expect non-DST */
		if (newtime.tm_isdst == 1 && (announce & eDT_AnnounceChDST) == 0 &&
		    utchour < 24) {
			rval |= eDT_DSTJump; /* sudden change */
			ok = false;
		}
	}
	/* done with DST */
	if (((announce & eDT_AnnounceChDST) == eDT_AnnounceChDST) && time->tm_min == 0) {
		announce &= ~eDT_AnnounceChDST;
		rval |= eDT_ChDST;
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
		if ((rval & eDT_DSTJump) == 0) {
			time->tm_isdst = newtime.tm_isdst;
			time->tm_gmtoff = newtime.tm_gmtoff;
		}
	}
	if (!ok)
		olderr = true;

	return rval | announce;
}
