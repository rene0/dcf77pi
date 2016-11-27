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

#include "bits1to14.h"
#include "calendar.h"
#include "config.h"
#include "decode_time.h"
#include "input.h"
#include "mainloop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

void
display_bit(uint16_t state, uint8_t bitpos)
{
	if (is_space_bit(bitpos))
		printf(" ");
	if ((state & eGB_receive) == eGB_receive)
		printf("r");
	else if ((state & eGB_transmit) == eGB_transmit)
		printf("x");
	else if ((state & eGB_random) == eGB_random)
		printf("#");
	else if ((state & eGB_read) == eGB_read)
		printf("_");
	else
		printf("%u", get_buffer()[bitpos]);
}

void
display_time(uint32_t dt, struct tm time)
{
	printf("%s %04d-%02d-%02d %s %02d:%02d\n",
	    time.tm_isdst == 1 ? "summer" : time.tm_isdst == 0 ? "winter" :
	    "?     ", time.tm_year, time.tm_mon, time.tm_mday,
	    weekday[time.tm_wday], time.tm_hour, time.tm_min);
	if ((dt & eDT_long) == eDT_long)
		printf("Minute too long\n");
	if ((dt & eDT_short) == eDT_short)
		printf("Minute too short\n");
	if ((dt & eDT_DST_error) == eDT_DST_error)
		printf("Time offset error\n");
	if ((dt & eDT_DST_jump) == eDT_DST_jump)
		printf("Time offset jump (ignored)\n");
	if ((dt & eDT_minute) == eDT_minute)
		printf("Minute parity/value error\n");
	if ((dt & eDT_minute_jump) == eDT_minute_jump)
		printf("Minute value jump\n");
	if ((dt & eDT_hour) == eDT_hour)
		printf("Hour parity/value error\n");
	if ((dt & eDT_hour_jump) == eDT_hour_jump)
		printf("Hour value jump\n");
	if ((dt & eDT_date) == eDT_date)
		printf("Date parity/value error\n");
	if ((dt & eDT_weekday_jump) == eDT_weekday_jump)
		printf("Day-of-week value jump\n");
	if ((dt & eDT_monthday_jump) == eDT_monthday_jump)
		printf("Day-of-month value jump\n");
	if ((dt & eDT_month_jump) == eDT_month_jump)
		printf("Month value jump\n");
	if ((dt & eDT_year_jump) == eDT_year_jump)
		printf("Year value jump\n");
	if ((dt & eDT_bit0) == eDT_bit0)
		printf("Minute marker error\n");
	if ((dt & eDT_bit20) == eDT_bit20)
		printf("Date/time start marker error\n");
	if ((dt & eDT_transmit) == eDT_transmit)
		printf("Transmitter call bit set\n");
	if ((dt & eDT_announce_ch_DST) == eDT_announce_ch_DST)
		printf("Time offset change announced\n");
	if ((dt & eDT_announce_leapsecond) == eDT_announce_leapsecond)
		printf("Leap second announced\n");
	if ((dt & eDT_ch_DST) == eDT_ch_DST)
		printf("Time offset changed\n");
	if ((dt & eDT_leapsecond) == eDT_leapsecond) {
		printf("Leap second processed");
		if ((dt & eDT_leapsecond_one) == eDT_leapsecond_one)
			printf(", value is 1 instead of 0");
		printf("\n");
	}
	if ((dt & eDT_ch_DST_error) == eDT_ch_DST_error)
		printf("Spurious time offset change announcement\n");
	if ((dt & eDT_leapsecond_error) == eDT_leapsecond_error)
		printf("Spurious leap second announcement\n");
	printf("\n");
}

void
display_alarm(struct alm alarm)
{
	uint8_t i;

	printf("German civil warning for: %s\n", get_region_name(alarm));
	for (i = 0; i < 2; i++)
		printf("%u Regions: %x %x %x %x parities %x %x\n", i,
		    alarm.region[i].r1, alarm.region[i].r2, alarm.region[i].r3,
		    alarm.region[i].r4, alarm.parity[i].ps, alarm.parity[i].pl);
}

void
display_unknown(void)
{
	printf("Unknown third party contents\n");
}

void
display_weather(void)
{
	printf("Meteotime weather\n");
}

void
display_long_minute(void)
{
	printf(" L");
}

void
display_minute(uint8_t minlen)
{
	uint32_t cutoff;

	cutoff = get_cutoff();
	printf(" (%lu) %u ", (unsigned long)get_acc_minlen(), minlen);
	if (cutoff == 0xffff)
		printf("?\n");
	else
		printf("%6.4f\n", cutoff / 1e4);
}

void
display_thirdparty_buffer(const uint8_t * const tpbuf)
{
	uint8_t i;

	printf("Third party buffer: ");
	for (i = 0; i < tpBufLen; i++)
		printf("%u", tpbuf[i]);
	printf("\n");
}

int
main(int argc, char *argv[])
{
	int res;
	char *logfilename;

	if (argc == 2)
		logfilename = strdup(argv[1]);
	else {
		printf("usage: %s infile\n", argv[0]);
		return EX_USAGE;
	}

	res = read_config_file(ETCDIR"/config.txt");
	if (res != 0) {
		/* non-existent file? */
		cleanup();
		free(logfilename);
		return res;
	}
	res = set_mode_file(logfilename);
	if (res != 0) {
		/* something went wrong */
		cleanup();
		return res;
	}

	mainloop(NULL, get_bit_file, display_bit, display_long_minute,
	    display_minute, NULL, display_alarm, display_unknown,
	    display_weather, display_time, display_thirdparty_buffer, NULL,
	    NULL, NULL);
	free(logfilename);
	return res;
}
