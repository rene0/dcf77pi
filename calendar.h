/*
Copyright (c) 2016 René Ladan. All rights reserved.

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

#ifndef DCF77PI_CALENDAR_H
#define DCF77PI_CALENDAR_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * The base year for {@link century_offset}.
 * Update every 400 years, now at 2300-01-01
 */
extern const uint16_t base_year;

/**
 * An array containing the day numbers of each month in a leap year.
 */
extern const uint16_t dayinleapyear[12];

/**
 * Textual representation of the day of week, with Monday = 1,
 * Sunday = 7, and 0 = "???".
 */
extern const char * const weekday[8];

/**
 * Determines if the year of the current time is a leap year.
 * @param time The current time.
 * @return The year of the current time is a leap year.
 */
bool isleap(struct tm time);

/**
 * Calculates the last day of the month of the current time.
 *
 * @param time The current time.
 * @return The last day of the month of the given time.
 */
uint8_t lastday(struct tm time);

/**
 * Calculates the century offset of the current time.
 *
 * The result should be multiplied by 100 and then be added to {@link base_year}.
 *
 * @param time The current time.
 * @return The century offset (0 to 3 or -1 if an error happened).
 */
int8_t century_offset(struct tm time);

/**
 * Calculates the hour in UTC from the given time.
 *
 * @param time The time to calculate the hour in UTC from.
 * @return The hour value in UTC.
 */
uint8_t get_utchour(struct tm time);

/**
 * Adds one minute to the current time. Leap years are handled. Note that the
 * year will fall back to {@link base_year} when it reaches
 * {@link base_year} + 400.
 *
 * @param time The current time to be increased with one minute.
 */
void add_minute(struct tm * const time);

/**
 * Substracts one minute to the current time. Leap years are handled. Note that
 * the year will fall back to {@link base_year} + 399 when it reaches
 * {@link base_year} - 1.
 *
 * @param time The current time to be decreased with one minute.
 */
void substract_minute(struct tm * const time);

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
