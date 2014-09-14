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

#ifndef DCF77PI_DECODE_TIME_H
#define DCF77PI_DECODE_TIME_H

#include <stdint.h>
#include <time.h>

/* update every 400 years, now at 2400-01-01 */
#define BASEYEAR	2000

/* daylight saving time error, bit 17 = bit 18 */
#define DT_DSTERR	(1 << 0)
/* minute value/parity error */
#define DT_MIN		(1 << 1)
/* hour value/parity error */
#define DT_HOUR		(1 << 2)
/* date value/parity error */
#define DT_DATE		(1 << 3)
/* bit 0 is 1 */
#define DT_B0		(1 << 4)
/* bit 20 is 0 */
#define DT_B20		(1 << 5)
/* too short minute */
#define DT_SHORT	(1 << 6)
/* too long minute */
#define DT_LONG		(1 << 7)
/* unexpected daylight saving time change */
#define DT_DSTJUMP	(1 << 8)
/* unexpected daylight saving time change announcement */
#define DT_CHDSTERR	(1 << 9)
/* unexpected leap second announcement */
#define DT_LEAPERR	(1 << 10)
/* unexpected minute value change */
#define DT_MINJUMP	(1 << 11)
/* unexpected hour value change */
#define DT_HOURJUMP	(1 << 12)
/* unexpected day value change */
#define DT_MDAYJUMP	(1 << 13)
/* unexpected day-of-week value change */
#define DT_WDAYJUMP	(1 << 14)
/* unexpected month value change */
#define DT_MONTHJUMP	(1 << 15)
/* unexpected year value change */
#define DT_YEARJUMP	(1 << 16)
/* leap second should always be zero if present */
#define DT_LEAPONE	(1 << 17)
/* transmitter call bit (15) set */
#define DT_XMIT		(1 << 18)
/* daylight saving time just changed */
#define DT_CHDST	(1 << 19)
/* leap second just processed */
#define DT_LEAP		(1 << 20)

/* daylight saving time change announced */
#define ANN_CHDST	(1 << 30)
/* leap second announced */
#define ANN_LEAP	(1 << 31)

/**
 * Initialize the month values from the configuration:
 * * summermonth, wintermonth: 1..12 or none for out-of-bound values
 *   These values indicate in which month a change to daylight-saving
 *   respectively normal time is allowed.
 * * leapsecmonths: 1..12 (1 or more), or none for out-of-bound values.
 *   These values indicate in which months a leap second is allowed.
 */
void init_time(void);

/**
 * Add one minute to the current time. Daylight saving time transitions and
 * leap years are handled. Note that the year will fall back to BASEYEAR when
 * it reaches BASEYEAR + 400.
 *
 * @param time The current time increased with one minute.
 */
void add_minute(struct tm *time);

/**
 * Decodes the current time from the internal bit buffer.
 *
 * The current time is first increased using add_minute(), and only if the
 * parities and other checks match these values are replaced by their
 * calculated counterparts.
 *
 * @param init Indicates whether the state of the decoder is initial:
 *   0 = normal, first two minute marks passed
 *   1 = first minute mark passed
 *   2 = (unused)
 *   3 = just starting
 * @param minlen The length of this minute in bits (normally 59 or 60 in
 *   case of a leap second).
 * @param buffer The bit buffer.
 * @param time The current time, to be updated.
 * @return The state of this minute, the combination of the various DT_* and
 *   ANN_* values that are applicable.
 */
uint32_t decode_time(int init, unsigned int minlen, uint8_t *buffer,
    struct tm *time);

/**
 * Calculates the hour in UTC from the given time.
 *
 * @param time The time to calculate the hour in UTC from.
 * @return The hour value in UTC.
 */
int get_utchour(struct tm time);

/**
 * Return a textual representation of the given day-of-week.
 *
 * @param wday The day of week (1 = Monday, 7 = Sunday)
 * @return The textual representation of wday ("Mon" .. "Sun")
 */
char *get_weekday(int wday);

/**
 * Functions for the accumulated minute length. This counter keeps the
 * estimated wall clock time in milliseconds since startup. This way short
 * minutes are accumulated into one minute.
 * It should be reset to 0 when a minute with the correct length is received.
 * For other minutes, 60,000 should be substracted.
 */

/**
 * Retrieve the current value of the accumulated minute length.
 *
 * @return The accumulated minute length in milliseconds.
 */
unsigned int get_acc_minlen(void);

/**
 * Reset the accumulated minute length to 0.
 */
void reset_acc_minlen(void);

/**
 * Add a given amount of milliseconds to the accumulated minute length.
 *
 * @param ms The amount to add in milliseconds
 */
void add_acc_minlen(unsigned int ms);
#endif
