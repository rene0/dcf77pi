// Copyright 2016-2917 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "calendar.h"

#include <stdio.h>
#include <sysexits.h>
#include <time.h>

static void
init_fwd_tm(struct tm * const time)
{
	time->tm_year = base_year;
	time->tm_mon = 1;
	time->tm_mday = 1;
	time->tm_wday = 1;
	time->tm_hour = 0;
	time->tm_min = 0;
	/* extra fields: */
	time->tm_sec = 0;
	time->tm_yday = 0;
	time->tm_isdst = -1;
	time->tm_zone = NULL;
}

static int
test_utc(char *name, int hours)
{
	struct tm time, time2, time3;
	time.tm_wday = 1;
	time.tm_isdst = hours - 1;
	time.tm_min = 0;
	/* initialize to UTC */
	init_fwd_tm(&time2);
	for (int i = 0; i < 60 * hours; i++)
		time2 = substract_minute(time2, false);

	for (time.tm_year = base_year; time.tm_year < base_year + 400; time.tm_year++) {
		for (time.tm_mon = 1; time.tm_mon < 13; time.tm_mon++) {
			int lday = lastday(time);
			for (time.tm_mday = 1; time.tm_mday <= lday; time.tm_mday++) {
				for (time.tm_hour = 0; time.tm_hour < 24;
				    time.tm_hour++) {
					time3 = get_utctime(time);
					if (time2.tm_year != time3.tm_year ||
					    time2.tm_mon != time3.tm_mon ||
					    time2.tm_mday != time3.tm_mday ||
					    time2.tm_wday != time3.tm_wday ||
					    time2.tm_hour != time3.tm_hour) {
						printf("%s: get_utchour %i:"
						    " %d-%d-%d,%d %d must be"
						    " %d-%d-%d,%d %d\n", name, hours,
						    time3.tm_year, time3.tm_mon,
						    time3.tm_mday, time3.tm_wday,
						    time3.tm_hour, time2.tm_year,
						    time2.tm_mon, time2.tm_mday,
						    time2.tm_wday, time2.tm_hour);
						return EX_SOFTWARE;
					}
					for (int i = 0; i < 60; i++)
						time2 = add_minute(time2, false);
				}
				if (++time.tm_wday == 8)
					time.tm_wday = 1;
			}
		}
	}
	return EX_OK;
}

int
main(int argc, char *argv[])
{
	struct tm time, time2;
	int i;

	/* base_year is currently 1900, and 1900-01-01 is a Monday */
	time.tm_wday = 1; /* Monday */

	/* isleeapyear() and lastday() are OK by definition */

	/* century_offset(): check for every date if it matches */
	for (int century = 0; century < 4; century++)
		for (time.tm_year = 0; time.tm_year < 100; time.tm_year++)
			for (time.tm_mon = 1; time.tm_mon < 13; time.tm_mon++) {
				int lday;

				time.tm_year += base_year + century * 100;
				lday = lastday(time);
				time.tm_year -= base_year + century * 100;
				for (time.tm_mday = 1; time.tm_mday <= lday;
				    time.tm_mday++) {
					int co;

					co = century_offset(time);
					if (co != century) {
						printf("%s: %d-%d-%d,%d : %d "
						    "should be %d\n", argv[0],
						    time.tm_year, time.tm_mon,
						    time.tm_mday, time.tm_wday,
						    co, century);
						return EX_SOFTWARE;
					}
					if (++time.tm_wday == 8)
						time.tm_wday = 1;
				}
			}

	/* add_minute(): check for every minute increase if it matches */
	init_fwd_tm(&time2);
	time.tm_wday = 1;
	for (time.tm_year = base_year; time.tm_year < base_year + 400; time.tm_year++) {
		for (time.tm_mon = 1; time.tm_mon < 13; time.tm_mon++) {
			int lday = lastday(time);
			for (time.tm_mday = 1; time.tm_mday <= lday; time.tm_mday++) {
				for (time.tm_hour = 0; time.tm_hour < 24;
				    time.tm_hour++) {
					for (time.tm_min = 0; time.tm_min < 60;
					    time.tm_min++) {
						if (time2.tm_year != time.tm_year ||
						    time2.tm_mon != time.tm_mon ||
						    time2.tm_mday != time.tm_mday ||
						    time2.tm_wday != time.tm_wday ||
						    time2.tm_hour != time.tm_hour ||
						    time2.tm_min != time.tm_min) {
							printf("%s: add_minute: "
							    "%d-%d-%d,%d %d:%d must be"
							    " %d-%d-%d,%d %d:%d\n",
							    argv[0],
							    time2.tm_year, time2.tm_mon,
							    time2.tm_mday, time2.tm_wday,
							    time2.tm_hour, time2.tm_min,
							    time.tm_year, time.tm_mon,
							    time.tm_mday, time.tm_wday,
							    time.tm_hour, time.tm_min);
							return EX_SOFTWARE;
						}
						time2 = add_minute(time2, false);
					}
				}
				if (++time.tm_wday == 8)
					time.tm_wday = 1;
			}
		}
	}

	/* substract_minute(): check for every minute increase if it matches */
	time2.tm_year = base_year + 399;
	time2.tm_mon = 12;
	time2.tm_mday = 31;
	time2.tm_wday = 7;
	time2.tm_hour = 23;
	time2.tm_min = 59;
	/* extra fields: */
	time2.tm_sec = 0;
	time2.tm_yday = 0;
	time2.tm_isdst = -1;
	time2.tm_zone = NULL;
	time.tm_wday = 7;
	for (time.tm_year = base_year + 399; time.tm_year >= base_year;
	    time.tm_year--) {
		for (time.tm_mon = 12; time.tm_mon > 0; time.tm_mon--) {
			int lday = lastday(time);
			for (time.tm_mday = lday; time.tm_mday > 0; time.tm_mday--) {
				for (time.tm_hour = 23; time.tm_hour >= 0;
				    time.tm_hour--) {
					for (time.tm_min = 59; time.tm_min >= 0;
					    time.tm_min--) {
						if (time2.tm_year != time.tm_year ||
						    time2.tm_mon != time.tm_mon ||
						    time2.tm_mday != time.tm_mday ||
						    time2.tm_wday != time.tm_wday ||
						    time2.tm_hour != time.tm_hour ||
						    time2.tm_min != time.tm_min) {
							printf("%s: substract_minute: "
							    " %d-%d-%d,%d %d:%d must be"
							    " %d-%d-%d,%d %d:%d\n",
							    argv[0],
							    time2.tm_year, time2.tm_mon,
							    time2.tm_mday, time2.tm_wday,
							    time2.tm_hour, time2.tm_min,
							    time.tm_year, time.tm_mon,
							    time.tm_mday, time.tm_wday,
							    time.tm_hour, time.tm_min);
							return EX_SOFTWARE;
						}
						time2 = substract_minute(time2, false);
					}
				}
				if (--time.tm_wday == 0)
					time.tm_wday = 7;
			}
		}
	}

	/* get_utchour(): check for each hour if it matches */
	i = test_utc(argv[0], 1);
	if (i != EX_OK)
		return i;
	i = test_utc(argv[0], 2);
	if (i != EX_OK)
		return i;

	/* get_isotime(): check for each minute or day if it matches */
	/* get_dcftime(): check for each minute or day if it matches */

	return EX_OK;
}
