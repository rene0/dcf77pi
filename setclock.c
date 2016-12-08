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

#include "setclock.h"

#include "calendar.h"
#include "decode_time.h"
#include "input.h"

#include <string.h>
#include <sys/time.h>

bool
setclock_ok(uint8_t init_min, const struct DT_result *dt, uint16_t bit)
{
	return init_min == 0 && dt->bit0_ok && dt->bit20_ok &&
	    dt->minute_length == emin_ok && dt->minute_status == eval_ok &&
	    dt->hour_status == eval_ok && dt->mday_status == eval_ok &&
	    dt->wday_status == eval_ok && dt->month_status == eval_ok &&
	    dt->year_status == eval_ok && dt->dst_announce != eann_error &&
	    (dt->dst_status == eDST_ok || dt->dst_status == eDST_done) &&
	    dt->leap_announce != eann_error &&
	    dt->leapsecond_status != els_one &&
	    ((bit & ~(eGB_one | eGB_EOM)) == 0);
}

int8_t
setclock(struct tm time)
{
	time_t epochtime;
	struct tm it;
	struct timeval tv;
	struct timezone tz;

	tzset(); /* does not help for TZ=UTC */

	if (time.tm_isdst == -1)
		return -1;
	it = get_isotime(time);
	it.tm_sec = 0;
	epochtime = mktime(&it);
	if (epochtime == -1)
		return -1;
	tv.tv_sec = epochtime;
	tv.tv_usec = 50000; /* adjust for bit reception algorithm */
	tz.tz_minuteswest = -60;
	tz.tz_dsttime = it.tm_isdst;
	return (settimeofday(&tv, &tz) == -1) ? (int8_t)-2 : (int8_t)0;
}
