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

#ifndef DCF77PI_DECODE_TIME_H
#define DCF77PI_DECODE_TIME_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

enum eDT {
/** daylight saving time error, bit 17 = bit 18 */
	eDT_DSTError = 1 << 0,
/** minute value/parity error */
	eDT_Minute = 1 << 1,
/** hour value/parity error */
	eDT_Hour = 1 << 2,
/** date value/parity error */
	eDT_Date = 1 << 3,
/**
 * bit 0 must always be 0 :
 * http://www.eecis.udel.edu/~mills/ntp/dcf77.html
 */
	eDT_Bit0 = 1 << 4,
/** bit 20 is 0 */
	eDT_Bit20 = 1 << 5,
/** too short minute */
	eDT_Short = 1 << 6,
/** too long minute */
	eDT_Long = 1 << 7,
/** unexpected daylight saving time change */
	eDT_DSTJump = 1 << 8,
/** unexpected daylight saving time change announcement */
	eDT_ChDSTError = 1 << 9,
/** unexpected leap second announcement */
	eDT_LeapSecondError = 1 << 10,
/** unexpected minute value change */
	eDT_MinuteJump = 1 << 11,
/** unexpected hour value change */
	eDT_HourJump = 1 << 12,
/** unexpected day value change */
	eDT_MonthDayJump = 1 << 13,
/** unexpected day-of-week value change */
	eDT_WeekDayJump = 1 << 14,
/** unexpected month value change */
	eDT_MonthJump = 1 << 15,
/** unexpected year value change */
	eDT_YearJump = 1 << 16,
/**
 * leap second should always be 0 if present :
 * http://www.ptb.de/cms/en/fachabteilungen/abt4/fb-44/ag-442/dissemination-of-legal-time/dcf77/dcf77-time-code.html
 */
	eDT_LeapSecondOne = 1 << 17,
/** transmitter call bit (15) set */
	eDT_Transmit = 1 << 18,
/** daylight saving time just changed */
	eDT_ChDST = 1 << 19,
/** leap second just processed */
	eDT_LeapSecond = 1 << 20,
/** daylight saving time change announced */
	eDT_AnnounceChDST = 1 << 21,
/** leap second announced */
	eDT_AnnounceLeapSecond = 1 << 22,
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

/**
 * Return a textual representation of the given day-of-week.
 *
 * @param wday The day of week (1 = Monday, 7 = Sunday)
 * @return The textual representation of wday ("Mon" .. "Sun")
 */
const char * const get_weekday(uint8_t wday);

/**
 * Convert the given time in ISO format to DCF77 format.
 *
 * @param isotime The time in ISO format to convert
 * @return The time in DCF77 format, with the tm_zone field left to NULL.
 */
struct tm get_dcftime(struct tm isotime);

/**
 * Convert the given time in DCF77 format to ISO format.
 *
 * @param dcftime The time in DCF77 format to convert
 * @return The time in ISO format, with the tm_zone field left to NULL.
 */
struct tm get_isotime(struct tm dcftime);

#endif
