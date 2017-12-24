// Copyright 2016-2017 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DCF77PI_CALENDAR_H
#define DCF77PI_CALENDAR_H

#include <stdbool.h>
#include <time.h>

/**
 * The base year for {@link century_offset}.
 * Update every 400 years, now at 2300-01-01
 */
extern const int base_year;

/**
 * An array containing the day numbers of each month in a leap year.
 */
extern const int dayinleapyear[12];

/**
 * Textual representation of the day of week, with Monday = 1,
 * Sunday = 7, and an unknown day being 0.
 */
extern const char * const weekday[8];

/**
 * Determines if the year of the current time is a leap year.
 * @param time The current time.
 * @return The year of the current time is a leap year.
 */
bool isleapyear(struct tm time);

/**
 * Calculates the last day of the month of the current time.
 *
 * @param time The current time.
 * @return The last day of the month of the given time.
 */
int lastday(struct tm time);

/**
 * Calculates the century offset of the current time.
 *
 * The result should be multiplied by 100 and then be added to
 * {@link base_year}.
 *
 * @param time The current time.
 * @return The century offset (0 to 3 or -1 if an error happened).
 */
int century_offset(struct tm time);

/**
 * Calculates the time in UTC from the given time.
 *
 * @param time The time to calculate the UTC time from.
 * @return The time in UTC, with tm_isdst set to -2.
 */
struct tm get_utctime(struct tm time);

/**
 * Adds one minute to the current time. Note that the
 * year will fall back to {@link base_year} when it reaches
 * {@link base_year} + 400.
 *
 * Leap years and switches to and from daylight saving time are taken into
 * account. The latter can be disabled by forcing {@link dst_changes} to false.
 *
 * @param time The current time to be increased with one minute.
 * @param dst_changes The daylight saving time is about to start or end.
 * @return The increased time.
 */
struct tm add_minute(struct tm time, bool dst_changes);

/**
 * Substracts one minute to the current time. Note that
 * the year will fall back to {@link base_year} + 399 when it reaches
 * {@link base_year} - 1.
 *
 * Leap years and switches to and from daylight saving time are taken into
 * account. The latter can be disabled by forcing {@link dst_changes} to false.
 *
 * @param time The current time to be decreased with one minute.
 * @param dst_changes The daylight saving time is about to start or end.
 * @return The decreased time.
 */
struct tm substract_minute(struct tm time, bool dst_changes);

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
