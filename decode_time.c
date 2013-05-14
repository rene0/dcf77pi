#include <stdio.h>
#include "decode_time.h"

uint8_t announce = 0; /* save DST change and leap second announcements */

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
	int rval = 0;

	if (minlen < 59)
		rval |= DT_SHORT;
	if (minlen > 60)
		rval |= DT_LONG;

	if (buffer[0] == 1)
		rval |= DT_B0;
	if (buffer[20] == 0)
		rval |= DT_B20;

	if (buffer[15] == 1)
		rval |= DT_XMIT;

	if (getpar(buffer, 21, 28)) {
		time->tm_min = (time->tm_min + 1) % 60;
		rval |= DT_MIN;
	} else
		time->tm_min = getbcd(buffer, 21, 27);

	if (getpar(buffer, 29, 35)) {
		if (time->tm_min == 0)
			time->tm_hour = (time->tm_hour + 1) % 24;
		rval |= DT_HOUR;
	} else
		time->tm_hour = getbcd(buffer, 29, 34);

	if (getpar(buffer, 36, 58)) {
		if (time->tm_min == 0 && time->tm_hour == 0)
			add_day(time);
		rval |= DT_DATE;
	} else {
		time->tm_mday = getbcd(buffer, 36, 41);
		time->tm_wday = getbcd(buffer, 42, 44) % 7;
		time->tm_mon = getbcd(buffer, 45, 49) - 1;
		time->tm_year = getbcd(buffer, 50, 57) + 100 * get_century() - 1900;
	}

	/* these flags are saved between invocations: */
	if (buffer[16] == 1)
		announce |= ANN_CHDST;
	if (buffer[19] == 1)
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
		} else
			rval |= DT_LONG;
	}

	if (buffer[17] == buffer[18])
		rval |= DT_DSTERR;
	else if (buffer[17] != time->tm_isdst) {
		if (init2 || ((announce & ANN_CHDST) && time->tm_min == 0))
			time->tm_isdst = buffer[17]; /* expected change */
		else
			rval |= DT_DSTERR; /* sudden change */
	}
	time->tm_gmtoff = time->tm_isdst ? 7200 : 3600;

	return rval;
}

void
display_time(int init2, int dt, struct tm oldtime, struct tm time)
{
	printf("%s %s", time.tm_isdst ? "summer" : "winter", asctime(&time));
	if (dt & DT_DSTERR)
		printf("Time offset error\n");
	if (dt & DT_MIN)
		printf("Minute parity error\n");
	if (!init2 && oldtime.tm_min != time.tm_min)
		printf("Minute value error\n");
	if (dt & DT_HOUR)
		printf("Hour parity error\n");
	if (!init2 && oldtime.tm_hour != time.tm_hour)
		printf("Hour value error\n");
	if (dt & DT_DATE)
		printf("Date parity error\n");
	if (!init2 && oldtime.tm_wday != time.tm_wday)
		printf("Day-of-week value error\n");
	if (!init2 && oldtime.tm_mday != time.tm_mday)
		printf("Day-of-month value error\n");
	if (!init2 && oldtime.tm_mon != time.tm_mon)
		printf("Month value error\n");
	if (!init2 && oldtime.tm_year != time.tm_year)
		printf("Year value error\n");
	if (dt & DT_B0)
		printf("Minute marker error\n");
	if (dt & DT_B20)
		printf("Date/time start marker error\n");
	if (dt & DT_XMIT)
		printf("Transmitter error\n");
	if (announce & ANN_CHDST)
		printf("Time offset change announced\n");
	if (announce & ANN_LEAP)
		printf("Leap second announced\n");
	if (dt & DT_CHDST)
		printf("Time offset changed\n");
	if (dt & DT_LEAP)
		printf("Leap second processed\n");
}
