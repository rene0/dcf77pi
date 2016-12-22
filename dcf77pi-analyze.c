/*
Copyright (c) 2013-2014, 2016 René Ladan. All rights reserved.

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
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"
#include "mainloop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

void
display_bit(struct GB_result bit, int bitpos)
{
	if (is_space_bit(bitpos))
		printf(" ");
	if (bit.hwstat == ehw_receive)
		printf("r");
	else if (bit.hwstat == ehw_transmit)
		printf("x");
	else if (bit.hwstat == ehw_random)
		printf("#");
	else if (bit.bitval == ebv_none)
		printf("_");
	else
		printf("%u", get_buffer()[bitpos]);
}

void
display_time(struct DT_result dt, struct tm time)
{
	printf("%s %04d-%02d-%02d %s %02d:%02d\n",
	    time.tm_isdst == 1 ? "summer" : time.tm_isdst == 0 ? "winter" :
	    "?     ", time.tm_year, time.tm_mon, time.tm_mday,
	    weekday[time.tm_wday], time.tm_hour, time.tm_min);
	if (dt.minute_length == emin_long)
		printf("Minute too long\n");
	else if (dt.minute_length == emin_short)
		printf("Minute too short\n");
	if (dt.dst_status == eDST_error)
		printf("Time offset error\n");
	else if (dt.dst_status == eDST_jump)
		printf("Time offset jump (ignored)\n");
	else if (dt.dst_status == eDST_done)
		printf("Time offset changed\n");
	if (dt.minute_status == eval_parity)
		printf("Minute parity error\n");
	else if (dt.minute_status == eval_bcd)
		printf("Minute value error\n");
	else if (dt.minute_status == eval_jump)
		printf("Minute value jump\n");
	if (dt.hour_status == eval_parity)
		printf("Hour parity error\n");
	else if (dt.hour_status == eval_bcd)
		printf("Hour value error\n");
	else if (dt.hour_status == eval_jump)
		printf("Hour value jump\n");
	if (dt.mday_status == eval_parity)
		printf("Date parity error\n");
	if (dt.wday_status == eval_bcd)
		printf("Day-of-week value error\n");
	else if (dt.wday_status == eval_jump)
		printf("Day-of-week value jump\n");
	if (dt.mday_status == eval_bcd)
		printf("Day-of-month value error\n");
	else if (dt.mday_status == eval_jump)
		printf("Day-of-month value jump\n");
	if (dt.month_status == eval_bcd)
		printf("Month value error\n");
	else if (dt.month_status == eval_jump)
		printf("Month value jump\n");
	if (dt.year_status == eval_bcd)
		printf("Year value error\n");
	else if (dt.year_status == eval_jump)
		printf("Year value jump\n");
	if (!dt.bit0_ok)
		printf("Minute marker error\n");
	if (!dt.bit20_ok)
		printf("Date/time start marker error\n");
	if (dt.transmit_call)
		printf("Transmitter call bit set\n");
	if (dt.dst_announce == eann_ok)
		printf("Time offset change announced\n");
	else if (dt.dst_announce == eann_error)
		printf("Spurious time offset change announcement\n");
	if (dt.leap_announce == eann_ok)
		printf("Leap second announced\n");
	else if (dt.leap_announce == eann_error)
		printf("Spurious leap second announcement\n");
	if (dt.leapsecond_status == els_done)
		printf("Leap second processed\n");
	else if (dt.leapsecond_status == els_one)
		printf("Leap second processed with value 1 instead of 0\n");
	printf("\n");
}

void
display_alarm(struct alm alarm)
{
	printf("German civil warning for: %s\n", get_region_name(alarm));
	for (unsigned i = 0; i < 2; i++)
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
display_minute(int minlen)
{
	int cutoff;

	cutoff = get_cutoff();
	printf(" (%u) %i ", get_acc_minlen(), minlen);
	if (cutoff == -1)
		printf("?\n");
	else
		printf("%6.4f\n", cutoff / 1e4);
}

void
display_thirdparty_buffer(const unsigned * const tpbuf)
{
	printf("Third party buffer: ");
	for (int i = 0; i < tpBufLen; i++)
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
