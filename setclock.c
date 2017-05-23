/*
Copyright (c) 2013-2014, 2016-2017 René Ladan. All rights reserved.

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

#include "setclock.h"

#include "calendar.h"
#include "decode_time.h"
#include "input.h"

#include <locale.h>
#include <string.h>
#include <time.h>

bool
setclock_ok(unsigned init_min, struct DT_result dt, struct GB_result bit)
{
	return init_min == 0 &&
	    dt.bit0_ok && dt.bit20_ok && dt.minute_length == emin_ok &&
	    dt.minute_status == eval_ok && dt.hour_status == eval_ok &&
	    dt.mday_status == eval_ok && dt.wday_status == eval_ok &&
	    dt.month_status == eval_ok && dt.year_status == eval_ok &&
	    dt.dst_announce != eann_error &&
	    (dt.dst_status == eDST_ok || dt.dst_status == eDST_done) &&
	    dt.leap_announce != eann_error &&
	    dt.leapsecond_status != els_one &&
	    !bit.bad_io && bit.bitval != ebv_none && bit.hwstat == ehw_ok &&
	    (bit.marker == emark_none || bit.marker == emark_minute);
}

int
setclock(struct tm settime)
{
	time_t epochtime, t1, t2;
	struct tm it;
	struct timespec ts;

	/* determine time difference of host to UTC (t1 - t2) */
	(void)time(&t1);
	(void)gmtime_r(&t1, &it);
	it.tm_isdst = -1;
	setlocale(LC_TIME, "");
	t2 = mktime(&it);
	if (t2 == -1)
		return -1;

	if (settime.tm_year >= base_year)
		it = get_isotime(settime);
	else
		memcpy((void *)&it, (const void *)&settime, sizeof(settime));
	it.tm_isdst = -1; /* allow mktime() when host timezone is UTC */
	it.tm_sec = 0;
	epochtime = mktime(&it);
	if (epochtime == -1)
		return -1;
	ts.tv_sec = epochtime;
	/* UTC if t1 == t2, so adjust from local time in that case */
	if (t1 == t2)
		ts.tv_sec -= 3600 * (1 + settime.tm_isdst);
	ts.tv_nsec = 50000000; /* adjust for bit reception algorithm */
	return (clock_settime(CLOCK_REALTIME, &ts) == -1) ? -2 : 0;
}
