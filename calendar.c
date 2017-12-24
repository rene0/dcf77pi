// Copyright 2016-2017 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "calendar.h"

#include <string.h>

const int base_year = 1900;

const int dayinleapyear[12] =
    {0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335};

const char * const weekday[8] =
    {"???", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

/* based on: xx00-02-28 is a Monday if and only if xx00 is a leap year */
int
century_offset(struct tm time)
{
	int d, nw, nd, wd;
	int tmp; /* resulting day of year, 02-28 if xx00 is leap */

	/* substract year days from weekday, including normal leap years */
	wd = (time.tm_wday - time.tm_year - time.tm_year / 4 -
	    (((time.tm_year % 4) > 0) ? 1 : 0)) % 7;
	if (wd < 1)
		wd += 7;

	/* weekday 1 is a Monday, assume this year is a leap year */
	/* if leap, we should reach Monday xx00-02-28 */
	d = dayinleapyear[time.tm_mon - 1] + time.tm_mday;
	if (d < 60) { /* at or before 02-28 (day 59) */
		nw = (59 - d) / 7;
		nd = wd == 1 ? 0 : (8 - wd);
		tmp = d + (nw * 7) + nd;
	} else { /* after 02-28 (day 59) */
		if ((time.tm_year % 4) > 0)
			d--; /* no 02-29 for obvious non-leap years */
		nw = (d - 59) / 7;
		nd = wd - 1;
		tmp = d - (nw * 7) - nd;
	}
	/* if day-in-year is 59, this year (xx00) is leap */
	if (tmp == 59)
		return 1;
	if (tmp == 53 || tmp == 54 || tmp == 60 || tmp == 61)
		return 2;
	if (tmp == 55 || tmp == 56 || tmp == 62 || tmp == 63)
		return 3;
	if (tmp == 57 || tmp == 58 || tmp == 64 || tmp == 65)
		return 0;
	return -1; /* ERROR */
}

bool
isleapyear(struct tm time)
{
	return (time.tm_year % 4 == 0 && time.tm_year % 100 != 0) ||
	    time.tm_year % 400 == 0;
}

int
lastday(struct tm time)
{
	if (time.tm_mon == 2)
		return 28 + (isleapyear(time) ? 1 : 0);
	if (time.tm_mon == 4 || time.tm_mon == 6 || time.tm_mon == 9 ||
	    time.tm_mon == 11)
		return 30;
	return 31;
}

static void
substract_hour(struct tm * const dt)
{
	if (--dt->tm_hour == -1) {
		dt->tm_hour = 23;
		if (--dt->tm_wday == 0)
			dt->tm_wday = 7;
		if (--dt->tm_mday == 0) {
			if (--dt->tm_mon == 0) {
				dt->tm_mon = 12;
				if (--dt->tm_year == base_year - 1)
					dt->tm_year = base_year + 399;
					/* bump! */
			}
			dt->tm_mday = lastday(*dt);
		}
	}
}

struct tm
get_utctime(struct tm time)
{
	struct tm dt;

	memcpy((void *)&dt, (const void*)&time, sizeof(time));
	if (time.tm_isdst == 0 || time.tm_isdst == 1) {
		substract_hour(&dt);
		if (time.tm_isdst == 1)
			substract_hour(&dt);
		dt.tm_isdst = -2; /* indicate UTC */
	}
	return dt;
}

struct tm
add_minute(struct tm time, bool dst_changes)
{
	struct tm dt;

	memcpy((void *)&dt, (const void*)&time, sizeof(time));
	if (++dt.tm_min == 60) {
		dt.tm_min = 0;
		if (dst_changes) {
			if (dt.tm_isdst == 1)
				dt.tm_hour--; /* will become non-DST */
			if (dt.tm_isdst == 0)
				dt.tm_hour++; /* will become DST */
		}
		if (++dt.tm_hour == 24) {
			dt.tm_hour = 0;
			if (++dt.tm_wday == 8)
				dt.tm_wday = 1;
			if (++dt.tm_mday > lastday(dt)) {
				dt.tm_mday = 1;
				if (++dt.tm_mon == 13) {
					dt.tm_mon = 1;
					if (++dt.tm_year == base_year + 400)
						dt.tm_year = base_year;
						/* bump! */
				}
			}
		}
	}
	return dt;
}

struct tm
substract_minute(struct tm time, bool dst_changes)
{
	struct tm dt;

	memcpy((void *)&dt, (const void*)&time, sizeof(time));
	if (--dt.tm_min == -1) {
		dt.tm_min = 59;
		if (dst_changes) {
			/* logic is backwards here */
			if (dt.tm_isdst == 1)
				dt.tm_hour++; /* will become DST */
			if (dt.tm_isdst == 0)
				dt.tm_hour--; /* will become non-DST */
		}
		substract_hour(&dt);
	}
	return dt;
}

struct tm
get_dcftime(struct tm isotime)
{
	struct tm dt;

	memcpy((void *)&dt, (const void*)&isotime, sizeof(isotime));
	dt.tm_mon++;
	dt.tm_year += 1900;
	if (dt.tm_wday == 0)
		dt.tm_wday = 7;
	dt.tm_yday = dayinleapyear[isotime.tm_mon] + dt.tm_mday;
	if (dt.tm_mon > 2 && !isleapyear(dt))
		dt.tm_yday--;

	return dt;
}

struct tm
get_isotime(struct tm dcftime)
{
	struct tm it;

	memcpy((void *)&it, (const void *)&dcftime, sizeof(dcftime));
	it.tm_mon--;
	it.tm_year -= 1900;
	if (it.tm_wday == 7)
		it.tm_wday = 0;
	it.tm_yday = dayinleapyear[it.tm_mon] + it.tm_mday - 1;
	if (dcftime.tm_mon > 2 && !isleapyear(dcftime))
		it.tm_yday--;

	return it;
}
