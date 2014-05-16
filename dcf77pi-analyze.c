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

#include "input.h"
#include "decode_time.h"
#include "decode_alarm.h"
#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

int bitpos;

void
display_bit_file(uint16_t state)
{
	if (is_space_bit(bitpos))
		printf(" ");
	if (state & GETBIT_RECV)
		printf("r");
	else if (state & GETBIT_XMIT)
		printf("x");
	else if (state & GETBIT_RND)
		printf("#");
	else if (state & GETBIT_READ)
		printf("_");
	else
		printf("%u", get_buffer()[bitpos]);
}

void
display_time_file(uint32_t dt, struct tm time)
{
	printf("%s %04d-%02d-%02d %s %02d:%02d\n",
	    time.tm_isdst ? "summer" : "winter", time.tm_year, time.tm_mon,
	    time.tm_mday, get_weekday(time.tm_wday), time.tm_hour, time.tm_min);
	if (dt & DT_LONG)
		printf("Minute too long\n");
	if (dt & DT_SHORT)
		printf("Minute too short\n");
	if (dt & DT_DSTERR)
		printf("Time offset error\n");
	if (dt & DT_DSTJUMP)
		printf("Time offset jump (ignored)\n");
	if (dt & DT_MIN)
		printf("Minute parity/value error\n");
	if (dt & DT_MINJUMP)
		printf("Minute value jump\n");
	if (dt & DT_HOUR)
		printf("Hour parity/value error\n");
	if (dt & DT_HOURJUMP)
		printf("Hour value jump\n");
	if (dt & DT_DATE)
		printf("Date parity/value error\n");
	if (dt & DT_WDAYJUMP)
		printf("Day-of-week value jump\n");
	if (dt & DT_MDAYJUMP)
		printf("Day-of-month value jump\n");
	if (dt & DT_MONTHJUMP)
		printf("Month value jump\n");
	if (dt & DT_YEARJUMP)
		printf("Year value jump\n");
	if (dt & DT_B0)
		printf("Minute marker error\n");
	if (dt & DT_B20)
		printf("Date/time start marker error\n");
	if (dt & DT_XMIT)
		printf("Transmitter call bit set\n");
	if (dt & ANN_CHDST)
		printf("Time offset change announced\n");
	if (dt & ANN_LEAP)
		printf("Leap second announced\n");
	if (dt & DT_CHDST)
		printf("Time offset changed\n");
	if (dt & DT_LEAP) {
		printf("Leap second processed");
		if (dt & DT_LEAPONE)
			printf(", value is 1 instead of 0");
		printf("\n");
	}
	if (dt & DT_CHDSTERR)
		printf("Spurious time offset change announcement\n");
	if (dt & DT_LEAPERR)
		printf("Spurious leap second announcement\n");
	printf("\n");
}

void
display_alarm_file(struct alm alarm)
{
	printf("German civil warning:"
	    " 0x%1x 0x%1x 0x%1x 0x%1x 0x%03x 0x%1x 0x%03x 0x%1x\n",
	    alarm.ds1, alarm.ps1, alarm.ds2, alarm.ps2,
	    alarm.dl1, alarm.pl1, alarm.dl2, alarm.pl2);
}

void
display_alarm_error_file(void)
{
	printf("Civil warning error\n");
}

int
main(int argc, char *argv[])
{
	uint16_t bit;
	struct tm time, oldtime;
	struct alm civwarn;
	int minlen = 0, acc_minlen = 0, old_acc_minlen;
	uint32_t dt = 0;
	int init = 1, init2 = 1;
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
		return res;
	}
	res = set_mode_file(logfilename);
	if (res != 0) {
		/* something went wrong */
		cleanup();
		return res;
	}
	init_time();

	/* no weird values please */
	bzero(&time, sizeof(time));
	init_alarm();

	for (;;) {
		bit = get_bit_file();
		if (bit & GETBIT_EOD)
			break;

		if (bit & (GETBIT_RECV | GETBIT_XMIT | GETBIT_RND))
			acc_minlen += 2500;
		else
			acc_minlen += 1000;

		bitpos = get_bitpos();
		if (bit & GETBIT_EOM) {
			/* handle the missing minute marker */
			minlen = bitpos + 1;
			acc_minlen += 1000;
		}
		display_bit_file(bit);

		if (init == 0)
			fill_civil_buffer(time.tm_min, bitpos, bit);

		bit = next_bit();
		if (bit & GETBIT_TOOLONG) {
			minlen = 61;
			/*
			 * leave acc_minlen alone,
			 * any missing marker already processed
			 */
			printf(" L");
		}

		if (bit & (GETBIT_EOM | GETBIT_TOOLONG)) {
			old_acc_minlen = acc_minlen;
			printf(" (%d) %d\n", acc_minlen, minlen);
			if (init == 1 || minlen >= 59)
				memcpy((void *)&oldtime, (const void *)&time,
				    sizeof(time));
			dt = decode_time(init, init2, minlen, get_buffer(),
			    &time, &acc_minlen);

			if (time.tm_min % 3 == 0 && init == 0) {
				decode_alarm(&civwarn);
				switch (get_civil_status()) {
				case 3:
					display_alarm_file(civwarn);
					break;
				case 2:
				case 1:
					display_alarm_error_file();
					break;
				case 0:
					break;
				}
			}

			display_time_file(dt, time);

			if (init == 1 || !((dt & DT_LONG) || (dt & DT_SHORT)))
				acc_minlen = 0; /* really a new minute */
			if (init == 0 && init2 == 1)
				init2 = 0;
			if (init == 1)
				init = 0;
		}
	}

	cleanup();
	free(logfilename);
	return 0;
}
