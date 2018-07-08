// Copyright 2013-2017 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "bits1to14.h"
#include "calendar.h"
#include "decode_alarm.h"
#include "decode_time.h"
#include "input.h"
#include "mainloop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

static void
display_bit(struct GB_result bit, int bitpos)
{
	if (is_space_bit(bitpos)) {
		printf(" ");
	}
	if (bit.hwstat == ehw_receive) {
		printf("r");
	} else if (bit.hwstat == ehw_transmit) {
		printf("x");
	} else if (bit.hwstat == ehw_random) {
		printf("#");
	} else if (bit.bitval == ebv_none) {
		printf("_");
	} else {
		printf("%i", get_buffer()[bitpos]);
	}
}

static void
display_time(struct DT_result dt, struct tm time)
{
	printf("%s %04d-%02d-%02d %s %02d:%02d\n",
	    time.tm_isdst == 1 ? "summer" : time.tm_isdst == 0 ? "winter" :
	    "?     ",
	    time.tm_year, time.tm_mon, time.tm_mday, weekday[time.tm_wday],
	    time.tm_hour, time.tm_min);
	if (dt.minute_length == emin_long) {
		printf("Minute too long\n");
	} else if (dt.minute_length == emin_short) {
		printf("Minute too short\n");
	}
	if (dt.dst_status == eDST_error) {
		printf("Time offset error\n");
	} else if (dt.dst_status == eDST_jump) {
		printf("Time offset jump (ignored)\n");
	} else if (dt.dst_status == eDST_done) {
		printf("Time offset changed\n");
	}
	if (dt.minute_status == eval_parity) {
		printf("Minute parity error\n");
	} else if (dt.minute_status == eval_bcd) {
		printf("Minute value error\n");
	} else if (dt.minute_status == eval_jump) {
		printf("Minute value jump\n");
	}
	if (dt.hour_status == eval_parity) {
		printf("Hour parity error\n");
	} else if (dt.hour_status == eval_bcd) {
		printf("Hour value error\n");
	} else if (dt.hour_status == eval_jump) {
		printf("Hour value jump\n");
	}
	if (dt.mday_status == eval_parity) {
		printf("Date parity error\n");
	}
	if (dt.wday_status == eval_bcd) {
		printf("Day-of-week value error\n");
	} else if (dt.wday_status == eval_jump) {
		printf("Day-of-week value jump\n");
	}
	if (dt.mday_status == eval_bcd) {
		printf("Day-of-month value error\n");
	} else if (dt.mday_status == eval_jump) {
		printf("Day-of-month value jump\n");
	}
	if (dt.month_status == eval_bcd) {
		printf("Month value error\n");
	} else if (dt.month_status == eval_jump) {
		printf("Month value jump\n");
	}
	if (dt.year_status == eval_bcd) {
		printf("Year value error\n");
	} else if (dt.year_status == eval_jump) {
		printf("Year value jump\n");
	}
	if (!dt.bit0_ok) {
		printf("Minute marker error\n");
	}
	if (!dt.bit20_ok) {
		printf("Date/time start marker error\n");
	}
	if (dt.transmit_call) {
		printf("Transmitter call bit set\n");
	}
	if (dt.dst_announce) {
		printf("Time offset change announced\n");
	}
	if (dt.leap_announce) {
		printf("Leap second announced\n");
	}
	if (dt.leapsecond_status == els_done) {
		printf("Leap second processed\n");
	} else if (dt.leapsecond_status == els_one) {
		printf("Leap second processed with value 1 instead of 0\n");
	}
	printf("\n");
}

static void
display_alarm(struct alm alarm)
{
	printf("German civil warning for: %s\n", get_region_name(alarm));
	for (unsigned i = 0; i < 2; i++) {
		printf("%u Regions: %x %x %x %x parities %x %x\n", i,
		    alarm.region[i].r1, alarm.region[i].r2, alarm.region[i].r3,
		    alarm.region[i].r4, alarm.parity[i].ps, alarm.parity[i].pl);
	}
}

static void
display_unknown(void)
{
	printf("Unknown third party contents\n");
}

static void
display_weather(void)
{
	printf("Meteotime weather\n");
}

static void
display_long_minute(void)
{
	printf(" L ");
}

static void
display_minute(int minlen)
{
	int cutoff;

	cutoff = get_cutoff();
	printf(" (%u) %i ", get_acc_minlen(), minlen);
	if (cutoff == -1) {
		printf("?\n");
	} else {
		printf("%6.4f\n", cutoff / 1e4);
	}
}

static void
display_thirdparty_buffer(const unsigned tpbuf[])
{
	printf("Third party buffer: ");
	for (int i = 0; i < TPBUFLEN; i++) {
		printf("%u", tpbuf[i]);
	}
	printf("\n");
}

int
main(int argc, char *argv[])
{
	int res;
	char *logfilename;

	if (argc == 2) {
		logfilename = strdup(argv[1]);
	} else {
		printf("usage: %s infile\n", argv[0]);
		return EX_USAGE;
	}

	res = set_mode_file(logfilename);
	if (res != 0) {
		/* something went wrong */
		cleanup();
		free(logfilename);
		return res;
	}

	mainloop(NULL, get_bit_file, display_bit, display_long_minute,
	    display_minute, NULL, display_alarm, display_unknown,
	    display_weather, display_time, display_thirdparty_buffer, NULL,
	    NULL, NULL);
	free(logfilename);
	return res;
}
