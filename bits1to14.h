// Copyright 2014-2018 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DCF77PI_BITS1TO14_H
#define DCF77PI_BITS1TO14_H

/** Length of the third-party buffer in bits */
#define TPBUFLEN 40

struct GB_result;

/** Indicates the type of the third party contents. */
enum eTP {
	/** unknown content */
	eTP_unknown,
	/** Meteotime weather (encrypted) */
	eTP_weather,
	/** German civil warning (unused) */
	eTP_alarm
};

/**
 * Add the current bit value to the third party buffer.
 *
 * @param minute The current value of the minute.
 * @param bitpos The current bit position.
 * @param gbr The current bit information.
 */
void fill_thirdparty_buffer(int minute, int bitpos, struct GB_result gbr);

/**
 * Retrieve the third party buffer.
 *
 * @return The third party buffer.
 */
const unsigned * get_thirdparty_buffer(void);

/**
 * Retrieve the type of the third party contents.
 *
 * @return The third party status.
 */
enum eTP get_thirdparty_type(void);

#endif
