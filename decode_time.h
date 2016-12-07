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

#include <stdint.h>
#include <time.h>

enum eDT {
/** daylight saving time error, bit 17 = bit 18 */
	eDT_DST_error = 1 << 0,
/** minute value/parity error */
	eDT_minute = 1 << 1,
/** hour value/parity error */
	eDT_hour = 1 << 2,
/** date value/parity error */
	eDT_date = 1 << 3,
/**
 * bit 0 must always be 0 :
 * http://www.eecis.udel.edu/~mills/ntp/dcf77.html
 */
	eDT_bit0 = 1 << 4,
/** bit 20 is 0 */
	eDT_bit20 = 1 << 5,
/** too short minute */
	eDT_short = 1 << 6,
/** too long minute */
	eDT_long = 1 << 7,
/** unexpected daylight saving time change */
	eDT_DST_jump = 1 << 8,
/** unexpected daylight saving time change announcement */
	eDT_ch_DST_error = 1 << 9,
/** unexpected leap second announcement */
	eDT_leapsecond_error = 1 << 10,
/** unexpected minute value change */
	eDT_minute_jump = 1 << 11,
/** unexpected hour value change */
	eDT_hour_jump = 1 << 12,
/** unexpected day value change */
	eDT_monthday_jump = 1 << 13,
/** unexpected day-of-week value change */
	eDT_weekday_jump = 1 << 14,
/** unexpected month value change */
	eDT_month_jump = 1 << 15,
/** unexpected year value change */
	eDT_year_jump = 1 << 16,
/**
 * leap second should always be 0 if present :
 * http://www.ptb.de/cms/en/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77/dcf77-time-code.html
 */
	eDT_leapsecond_one = 1 << 17,
/** transmitter call bit (15) set */
	eDT_transmit = 1 << 18,
/** daylight saving time just changed */
	eDT_ch_DST = 1 << 19,
/** leap second just processed */
	eDT_leapsecond = 1 << 20,
/** daylight saving time change announced */
	eDT_announce_ch_DST = 1 << 21,
/** leap second announced */
	eDT_announce_leapsecond = 1 << 22,
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
 * @return The state of this minute, the combination of the various DT_* and
 *   ANN_* values that are applicable.
 */
uint32_t decode_time(uint8_t init_min, uint8_t minlen, uint32_t acc_minlen,
    const uint8_t * const buffer, struct tm * const time);

#endif
