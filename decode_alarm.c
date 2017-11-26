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

#include "decode_alarm.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char * const reg1n = "SWH, HH, NS, BR, MVP";
static const char * const reg1m = "NRW, SA, BRA, B, TH, S";
static const char * const reg1s = "RP, SAA, HS, BW, BYN, BYS";

void
decode_alarm(const unsigned civbuf[], struct alm * const alarm)
{
	/* Partial information only, no parity checks */

	for (unsigned i = 0; i < 2; i++) {
		alarm->region[i].r1 = civbuf[6 * i] +
		    2 * civbuf[1 + 6 * i] +
		    4 * civbuf[3 + 6 * i];
		alarm->region[i].r2 = civbuf[12 + 14 * i] +
		    2 * civbuf[13 + 14 * i] +
		    4 * civbuf[14 + 14 * i];
		alarm->region[i].r3 = civbuf[15 + 14 * i] +
		    2 * civbuf[16 + 14 * i] +
		    4 * civbuf[17 + 14 * i];
		alarm->region[i].r4 = civbuf[19 + 14 * i] +
		    2 * civbuf[20 + 14 * i] +
		    4 * civbuf[21 + 14 * i] +
		    8 * civbuf[23 + 14 * i];

		alarm->parity[i].ps = civbuf[2 + 6 * i] +
		    2 * civbuf[4 + 6 * i] +
		    4 * civbuf[5 + 6 * i];
		alarm->parity[i].pl = civbuf[18 + 14 * i] +
		    2 * civbuf[22 + 14 * i] +
		    4 * civbuf[24 + 14 * i] +
		    8 * civbuf[25 + 14 * i];
	}
}

const char * const
get_region_name(struct alm alarm)
{
	char *res;
	bool need_comma;

	/* Partial information only */

	if (alarm.region[0].r1 != alarm.region[1].r1 ||
	    alarm.parity[0].ps != alarm.parity[1].ps)
		return "(inconsistent)";

	res = calloc(1, strlen(reg1n) + 2 + strlen(reg1m) + 2 + strlen(reg1s) + 1);

	if (res == NULL)
		return res;

	need_comma = false;
	if ((alarm.region[0].r1 & 1) == 1) {
		res = strcat(res, reg1n);
		need_comma = true;
	}
	if ((alarm.region[0].r1 & 2) == 2) {
		if (need_comma)
			res = strcat(res, ", ");
		res = strcat(res, reg1m);
		need_comma = true;
	}
	if ((alarm.region[0].r1 & 4) == 4) {
		if (need_comma)
			res = strcat(res, ", ");
		res = strcat(res, reg1s);
	}
	return res;
}
