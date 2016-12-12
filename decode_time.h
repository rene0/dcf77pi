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

#ifndef DCF77PI_DECODE_TIME_H
#define DCF77PI_DECODE_TIME_H

#include <stdbool.h>
#include <stdint.h>
struct tm;

enum eDT_length {
	/** minute length ok */
	emin_ok,
	/** minute too short */
	emin_short,
	/** minute too long */
	emin_long
};

enum eDT_tval {
	/** value ok */
	eval_ok,
	/** bcd error */
	eval_bcd,
	/** parity error */
	eval_parity,
	/** value ok but jumped */
	eval_jump
};

enum eDT_announce {
	/** no announcement */
	eann_none,
	/** unexpected announcement */
	eann_error,
	/** announcement ok */
	eann_ok
};

enum eDT_DST {
	/** daylight saving time ok */
	eDST_ok,
	/** daylight saving time error, bit 17 = bit 18 */
	eDST_error,
	/** unexpected daylight saving time change */
	eDST_jump,
	/** daylight saving time just changed */
	eDST_done
};

enum eDT_leapsecond {
	/** no leap second */
	els_none,
	/**
	 * leap second should always be 0 if present :
	 * http://www.ptb.de/cms/en/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77/dcf77-time-code.html
	 */
	els_one,
	/** leap second just processed */
	els_done
};

struct DT_result {
	/**
	 * bit 0 must always be 0 :
	 * http://www.eecis.udel.edu/~mills/ntp/dcf77.html
	 */
	bool bit0_ok;
	/** transmitter call bit (15) set */
	bool transmit_call;
	/** bit 20 must always be 1 */
	bool bit20_ok;
	/** minute length ok ? */
	enum eDT_length minute_length;
	/** minute value ok ? */
	enum eDT_tval minute_status;
	/** hour value ok ? */
	enum eDT_tval hour_status;
	/** day value ok ? */
	enum eDT_tval mday_status;
	/** weekday value ok ? */
	enum eDT_tval wday_status;
	/** month value ok ? */
	enum eDT_tval month_status;
	/** year value ok ? */
	enum eDT_tval year_status;
	/** DST ok ? */
	enum eDT_DST dst_status;
	/** leap second ok ? */
	enum eDT_leapsecond leapsecond_status;
	/** DST announcement ok ? */
	enum eDT_announce dst_announce;
	/** leap second announcement ok ? */
	enum eDT_announce leap_announce;
};

/**
 * Initialize the month values from the configuration:
 * - summermonth, wintermonth: 1..12 or none for out-of-bound values
 *   These values indicate in which month a change to daylight-saving
 *   respectively normal time is allowed.
 * - leapsecmonths: 1..12 (1 or more), or none for out-of-bound values.
 *   These values indicate in which months a leap second is allowed.
 */
void init_time(void);

/**
 * Decodes the current time from the internal bit buffer.
 *
 * The current time is first increased using add_minute(), and only if the
 * parities and other checks match these values are replaced by their
 * calculated counterparts.
 *
 * @param init_min Indicates whether the state of the decoder is initial:
 *   0 = normal, first two minute marks passed
 *   1 = first minute mark passed
 *   2 = just starting
 * @param minlen The length of this minute in bits (normally 59 or 60 in
 *   case of a leap second).
 * @param acc_minlen The accumulated minute length of this minute in
 *   milliseconds.
 * @param buffer The bit buffer.
 * @param time The current time, to be updated.
 * @return A structure containing the results of all the checks performed on
 *   the calculated time.
 */
const struct DT_result * const decode_time(uint8_t init_min, uint8_t minlen,
    uint32_t acc_minlen, const uint8_t * const buffer, struct tm * const time);

#endif
