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
#include <time.h>

static int summermonth, wintermonth;
static int leapsecmonths[12];
static int num_leapsecmonths;
static struct DT_result dt_res;

void
init_time(void)
{
	char *freeptr, *lsm, *mon;

	summermonth = (int)strtol(get_config_value("summermonth"), NULL, 10);
	if (summermonth < 1 || summermonth > 12)
		summermonth = 0;
	wintermonth = (int)strtol(get_config_value("wintermonth"), NULL, 10);
	if (wintermonth < 1 || wintermonth > 12)
		wintermonth = 0;

	freeptr = lsm = strdup(get_config_value("leapsecmonths"));
	num_leapsecmonths = 0;
	for (int i = 0; (mon = strsep(&lsm, ",")) != NULL; i++) {
		int m = (int)strtol(mon, NULL, 10);
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
	/*
	 * Local time is 1 or 2 hours ahead of UTC, which is what the
	 * configuration file uses, so adjust for that.
	 */
	if (--time.tm_mon == 0)
		time.tm_mon = 12;
	for (int i = 0; i < num_leapsecmonths; i++)
		if (leapsecmonths[i] == time.tm_mon)
			return true;
	return false;
}

static bool
getpar(const int * const buffer, unsigned start, unsigned stop)
{
	int par = 0;

	for (unsigned i = start; i <= stop; i++)
		par += buffer[i];
	return (par & 1) == 0;
}

static int
getbcd(const int * const buffer, unsigned start, unsigned stop)
{
	int mul = 1, val = 0;

	for (unsigned i = start; i <= stop; i++) {
		val += mul * buffer[i];
		mul *= 2;
	}
	return val;
}

static bool
check_time_sanity(int minlen, const int * const buffer)
{
	if (minlen < 59)
		dt_res.minute_length = emin_short;
	else if (minlen > 60)
		dt_res.minute_length = emin_long;
	else
		dt_res.minute_length = emin_ok;

	dt_res.bit0_ok = buffer[0] == 0;
	dt_res.bit20_ok = buffer[20] == 1;

	if (buffer[17] == buffer[18])
		dt_res.dst_status = eDST_error;
	else
		dt_res.dst_status = eDST_ok;

	/* only decode if set */
	return (dt_res.minute_length == emin_ok) && dt_res.bit0_ok &&
	    dt_res.bit20_ok && (dt_res.dst_status == eDST_ok);
}

static void
handle_special_bits(const int * const buffer)
{
	dt_res.transmit_call = buffer[15] == 1;
}

static int
increase_old_time(unsigned init_min, int minlen, unsigned acc_minlen,
    struct tm * const time)
{
	static unsigned acc_minlen_partial, old_acc_minlen;
	static bool prev_toolong;

	int increase;

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
		increase = (acc_minlen - old_acc_minlen) / 60000;
	else
		increase = acc_minlen / 60000;
	if (acc_minlen >= 60000)
		acc_minlen_partial %= 60000;
	/* Account for complete minutes with a short acc_minlen: */
	if (acc_minlen % 60000 > 59000) {
		increase++;
		acc_minlen_partial %= 60000;
	}

	prev_toolong = (minlen == -1);
	old_acc_minlen = acc_minlen - (acc_minlen % 60000);

	/* There is no previous time on the very first (partial) minute: */
	if (init_min < 2) {
		for (int i = increase; increase > 0 && i > 0; i--)
			add_minute(time, summermonth, wintermonth);
		for (int i = increase; increase < 0 && i < 0; i++)
			substract_minute(time, summermonth, wintermonth);
	}
	return increase;
}

static unsigned
calculate_date_time(unsigned init_min, unsigned errflags, int increase,
    const int * const buffer, const struct tm time, struct tm * const newtime)
{
	int tmp0, tmp1, tmp2, tmp3, tmp4, tmp5;
	bool p1, p2, p3;

	p1 = getpar(buffer, 21, 28);
	tmp0 = getbcd(buffer, 21, 24);
	tmp1 = getbcd(buffer, 25, 27);
	if (!p1)
		dt_res.minute_status = eval_parity;
	else if (tmp0 > 9 || tmp1 > 5) {
		dt_res.minute_status = eval_bcd;
		p1 = false;
	} else
		dt_res.minute_status = eval_ok;
	if ((init_min == 2 || increase != 0) && p1 && errflags == 0) {
		newtime->tm_min = tmp0 + 10 * tmp1;
		if (init_min == 0 && time.tm_min != newtime->tm_min)
			dt_res.minute_status = eval_jump;
	}

	p2 = getpar(buffer, 29, 35);
	tmp0 = getbcd(buffer, 29, 32);
	tmp1 = getbcd(buffer, 33, 34);
	if (!p2)
		dt_res.hour_status = eval_parity;
	else if (tmp0 > 9 || tmp1 > 2 || tmp0 + 10 * tmp1 > 23) {
		dt_res.hour_status = eval_bcd;
		p2 = false;
	} else
		dt_res.hour_status = eval_ok;
	if ((init_min == 2 || increase != 0) && p2 && errflags == 0) {
		newtime->tm_hour = tmp0 + 10 * tmp1;
		if (init_min == 0 && time.tm_hour != newtime->tm_hour)
			dt_res.hour_status = eval_jump;
	}

	p3 = getpar(buffer, 36, 58);
	tmp0 = getbcd(buffer, 36, 39);
	tmp1 = getbcd(buffer, 40, 41);
	tmp2 = getbcd(buffer, 42, 44);
	tmp3 = getbcd(buffer, 45, 48);
	tmp4 = getbcd(buffer, 50, 53);
	tmp5 = getbcd(buffer, 54, 57);
	if (!p3) {
		dt_res.mday_status = eval_parity;
		dt_res.wday_status = eval_parity;
		dt_res.month_status = eval_parity;
		dt_res.year_status = eval_parity;
	} else {
		if (tmp0 > 9 || tmp0 + 10 * tmp1 == 0 ||
		    tmp0 + 10 * tmp1 > 31) {
			dt_res.mday_status = eval_bcd;
			p3 = false;
		} else
			dt_res.mday_status = eval_ok;
		if (tmp2 == 0) {
			dt_res.wday_status = eval_bcd;
			p3 = false;
		} else
			dt_res.wday_status = eval_ok;
		if (tmp3 > 9 || tmp3 + 10 * buffer[49] == 0 ||
		    tmp3 + 10 * buffer[49] > 12) {
			dt_res.month_status = eval_bcd;
			p3 = false;
		} else
			dt_res.month_status = eval_ok;
		if (tmp4 > 9 || tmp5 > 9) {
			dt_res.year_status = eval_bcd;
			p3 = false;
		} else
			dt_res.year_status = eval_ok;
	}
	if ((init_min == 2 || increase != 0) && p3 && errflags == 0) {
		int centofs;

		newtime->tm_mday = tmp0 + 10 * tmp1;
		if (init_min == 0 && time.tm_mday != newtime->tm_mday)
			dt_res.mday_status = eval_jump;
		newtime->tm_wday = tmp2;
		if (init_min == 0 && time.tm_wday != newtime->tm_wday)
			dt_res.wday_status = eval_jump;
		newtime->tm_mon = tmp3 + 10 * buffer[49];
		if (init_min == 0 && time.tm_mon != newtime->tm_mon)
			dt_res.month_status = eval_jump;
		newtime->tm_year = tmp4 + 10 * tmp5;
		centofs = century_offset(*newtime);
		if (centofs == -1) {
			dt_res.year_status = eval_bcd;
			p3 = false;
		} else {
			if (init_min == 0 && time.tm_year !=
			    base_year + 100 * centofs + newtime->tm_year)
				dt_res.year_status = eval_jump;
			newtime->tm_year += base_year + 100 * centofs;
			if (newtime->tm_mday > lastday(*newtime)) {
				dt_res.mday_status = eval_bcd;
				p3 = false;
			}
		}
	}
	return (errflags << 3) | ((!p3) << 2) | ((!p2) << 1) | (!p1);
}

static void
stamp_date_time(unsigned errflags, const struct tm newtime,
    struct tm * const time)
{
	if ((errflags & 0x0f) == 0) {
		if ((errflags & 1) == 0)
			time->tm_min = newtime.tm_min;
		if ((errflags & 2) == 0)
			time->tm_hour = newtime.tm_hour;
		if ((errflags & 3) == 0) {
			time->tm_mday = newtime.tm_mday;
			time->tm_mon = newtime.tm_mon;
			time->tm_year = newtime.tm_year;
			time->tm_wday = newtime.tm_wday;
		}
		if (dt_res.dst_status != eDST_jump) {
			time->tm_isdst = newtime.tm_isdst;
			time->tm_gmtoff = newtime.tm_gmtoff;
		}
	}
}

static unsigned
handle_leap_second(unsigned errflags, int minlen, unsigned utchour,
    const int * const buffer, const struct tm time)
{
	/*
	 * h==23, last day of month (UTC) or h==0, first day of next month (UTC)
	 * according to IERS Bulletin C
	 * flag still set at 00:00 UTC, prevent leapsecond announcement error
	 */
	if (buffer[19] == 1 && errflags == 0) {
		if (time.tm_mday == 1 && is_leapsecmonth(time) &&
		    ((time.tm_min > 0 && utchour == 23) ||
		    (time.tm_min == 0 && utchour == 0)))
			dt_res.leap_announce = eann_ok;
		else
			dt_res.leap_announce = eann_error;
	}

	/* process possible leap second, always reset announcement at hh:00 */
	if (dt_res.leap_announce == eann_ok && time.tm_min == 0) {
		dt_res.leap_announce = eann_none;
		dt_res.leapsecond_status = els_done;
		if (minlen == 59) {
			/* leap second processed, but missing */
			dt_res.minute_length = emin_short;
			errflags |= (1 << 4);
		} else if (minlen == 60 && buffer[59] == 1)
			dt_res.leapsecond_status = els_one;
	}
	if (minlen == 60 && dt_res.leapsecond_status == els_none) {
		/* leap second not processed, so bad minute */
		dt_res.minute_length = emin_long;
		errflags |= (1 << 4);
	}
	return errflags;
}

static unsigned
handle_dst(unsigned errflags, bool olderr, unsigned utchour,
    const int * const buffer, const struct tm time,
    struct tm * const newtime)
{
	/*
	 * h==0 (UTC) because sz->wz -> h==2 and wz->sz -> h==1,
	 * last Sunday of month (reference?)
	 */
	if (buffer[16] == 1 && errflags == 0) {
		if ((time.tm_wday == 7 && time.tm_mday > lastday(time) - 7 &&
		    (time.tm_mon == summermonth ||
		    time.tm_mon == wintermonth)) && ((time.tm_min > 0 &&
		    utchour == 0) || (time.tm_min == 0 &&
		    utchour == 1 + buffer[17] - buffer[18])))
			dt_res.dst_announce = eann_ok; /* time zone just changed */
		else {
			dt_res.dst_announce = eann_none;
			dt_res.dst_status = eDST_error;
		}
	}

	if (buffer[17] != time.tm_isdst || buffer[18] == time.tm_isdst) {
		/*
		 * Time offset change is OK if:
		 * - announced and time is Sunday, lastday, 01:00 UTC
		 * - there was an error but not any more (needed if decoding at
		 *   startup is problematic)
		 * - initial state (otherwise DST would never be valid)
		 */
		if ((dt_res.dst_announce == eann_ok && time.tm_min == 0) ||
		    (olderr && errflags == 0) ||
		    (dt_res.dst_status == eDST_ok && time.tm_isdst == -1))
			newtime->tm_isdst = buffer[17]; /* expected change */
		else {
			//XXX klopt deze if?
			if (dt_res.dst_status != eDST_error)
				dt_res.dst_status = eDST_jump;
				/* sudden change, ignore */
			errflags |= (1 << 5);
		}
	}
	/* check if DST is within expected date range */
	if ((time.tm_mon > summermonth && time.tm_mon < wintermonth) ||
	    (time.tm_mon == summermonth && time.tm_wday < 7 &&
	      lastday(time) - time.tm_mday < 7) ||
	    (time.tm_mon == summermonth && time.tm_wday == 7 &&
	      lastday(time) - time.tm_mday < 7 && utchour > 0) ||
	    (time.tm_mon == wintermonth && time.tm_wday < 7 &&
	      lastday(time) - time.tm_mday >= 7) ||
	    (time.tm_mon == wintermonth && time.tm_wday == 7 &&
	      lastday(time) - time.tm_mday < 7 &&
		(utchour >= 22 /* previous day */ || utchour == 0))) {
		/* expect DST */
		if (newtime->tm_isdst == 0 && dt_res.dst_announce != eann_ok &&
		    utchour < 24) {
			dt_res.dst_status = eDST_jump; /* sudden change */
			errflags |= (1 << 5);
		}
	} else {
		/* expect non-DST */
		if (newtime->tm_isdst == 1 && dt_res.dst_announce != eann_ok &&
		    utchour < 24) {
			dt_res.dst_status = eDST_jump; /* sudden change */
			errflags |= (1 << 5);
		}
	}
	newtime->tm_gmtoff = 3600 * (newtime->tm_isdst + 1);

	/* done with DST */
	if (dt_res.dst_announce == eann_ok && time.tm_min == 0) {
		dt_res.dst_announce = eann_none;
		dt_res.dst_status = eDST_done;
	}

	return errflags;
}

const struct DT_result * const
decode_time(unsigned init_min, int minlen, unsigned acc_minlen,
    const int * const buffer, struct tm * const time)
{
	static bool olderr;

	unsigned utchour;
	unsigned errflags;
	int increase;
	struct tm newtime;

	memset(&newtime, 0, sizeof(newtime));
	/* Initially, set time offset to unknown */
	if (init_min == 2) {
		time->tm_isdst = -1;
		dt_res.dst_announce = eann_none;
		dt_res.leap_announce = eann_none;
	}
	newtime.tm_isdst = time->tm_isdst; /* save DST value */

	errflags = check_time_sanity(minlen, buffer) ? 0 : 1;

	if (errflags == 0)
		handle_special_bits(buffer);

	increase = increase_old_time(init_min, minlen, acc_minlen, time);

	errflags = calculate_date_time(init_min, errflags, increase, buffer,
	    *time, &newtime);

	utchour = get_utchour(*time);

	errflags = handle_leap_second(errflags, minlen, utchour, buffer, *time);

	errflags = handle_dst(errflags, olderr, utchour, buffer, *time,
	    &newtime);

	stamp_date_time(errflags, newtime, time);

	if (olderr && (errflags == 0))
		olderr = false;
	if (errflags != 0)
		olderr = true;

	return &dt_res;
}
