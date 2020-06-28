// Copyright 2013-2019 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "setclock.h"

#include "calendar.h"
#include "decode_time.h"
#include "input.h"

#include <locale.h>
#include <string.h>
#include <time.h>

bool
setclock_ok(unsigned init_min, struct DT_result dt, struct GB_result gbr)
{
	return init_min == 0 && gbr.marker == emark_minute && dt.bit0_ok &&
	    dt.bit20_ok && dt.minute_length == emin_ok &&
	    dt.minute_status == eval_ok && dt.hour_status == eval_ok &&
	    dt.mday_status == eval_ok && dt.wday_status == eval_ok &&
	    dt.month_status == eval_ok && dt.year_status == eval_ok &&
	    (dt.dst_status == eDST_ok || dt.dst_status == eDST_done) &&
	    dt.leapsecond_status != els_one && !gbr.bad_io &&
	    gbr.bitval != ebv_none && gbr.hwstat == ehw_ok;
}

enum eSC_status
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
	if (t2 == -1) {
		return esc_invalid;
	}

	if (settime.tm_year >= base_year) {
		it = get_isotime(settime);
	} else {
		memcpy((void *)&it, (const void *)&settime, sizeof(settime));
	}
	it.tm_isdst = -1; /* allow mktime() when host timezone is UTC */
	it.tm_sec = 0;
	epochtime = mktime(&it);
	if (epochtime == -1) {
		return esc_invalid;
	}
	ts.tv_sec = epochtime;
	/* UTC if t1 == t2, so adjust from local time in that case */
	if (t1 == t2) {
		ts.tv_sec -= 3600 * (1 + settime.tm_isdst);
	}
	ts.tv_nsec = 50000000; /* adjust for bit reception algorithm */
	return (clock_settime(CLOCK_REALTIME, &ts) == -1) ? esc_fail : esc_ok;
}
