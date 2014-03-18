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

#include "setclock.h"

#include "guifuncs.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

int
setclock(WINDOW *win, struct tm time)
{
	time_t epochtime;
	struct tm isotime;
	struct timeval tv;
	struct timezone tz;
	struct thread_info tinfo;

	tzset(); /* does not help for TZ=UTC */

	memcpy((void *)&isotime, (const void *)&time, sizeof(time));
	isotime.tm_year -= 1900;
	isotime.tm_mon--;
	isotime.tm_wday %= 7;
	isotime.tm_sec = 0;
	epochtime = mktime(&isotime);
	if (epochtime == -1) {
		tinfo.win = win;
		tinfo.str = strdup("mktime() failed!");
		pthread_create(&tinfo.id, NULL, statusbar, (void *)&tinfo);
		free(tinfo.str);
		return -1;
	}
	tv.tv_sec = epochtime;
	tv.tv_usec = 50000; /* adjust for bit reception algorithm */
	tinfo.win = win;
	asprintf(&tinfo.str, "Setting time (%lld , %lld)",
	    (long long int)tv.tv_sec, (long long int)tv.tv_usec);
	if (pthread_create(&tinfo.id, NULL, statusbar, (void *)&tinfo)) {
		free(tinfo.str);
		return -1;
	}
	free(tinfo.str);
	tz.tz_minuteswest = -60;
	tz.tz_dsttime = isotime.tm_isdst;
	if (settimeofday(&tv, &tz) == -1) {
		tinfo.win = win;
		asprintf(&tinfo.str, "settimeofday: %s", strerror(errno));
		if (pthread_create(&tinfo.id, NULL, statusbar,
		    (void *)&tinfo)) {
			free(tinfo.str);
			return -1;
		}
		free(tinfo.str);
		return -1;
	}
	return 0;
}
