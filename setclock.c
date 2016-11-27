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

#include "setclock.h"

#include "decode_time.h"
#include "input.h"

#include <string.h>
#include <sys/time.h>

bool
setclock_ok(uint8_t init_min, uint32_t dt, uint16_t bit)
{
	return init_min == 0 && ((dt & ~(DT_XMIT | DT_CHDST | DT_LEAP)) == 0) &&
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
	epochtime = mktime(&it);
	if (epochtime == -1)
		return -1;
	tv.tv_sec = epochtime;
	tv.tv_usec = 50000; /* adjust for bit reception algorithm */
	tz.tz_minuteswest = -60;
	tz.tz_dsttime = it.tm_isdst;
	return (settimeofday(&tv, &tz) == -1) ? (int8_t)-2 : (int8_t)0;
}
