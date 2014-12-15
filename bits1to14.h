/*
Copyright (c) 2014 René Ladan. All rights reserved.

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

#ifndef DCF77PI_BITS1TO14_H
#define DCF77PI_BITS1TO14_H

/** Length of the third-party buffer in bits */
#define TPBUFLEN 40

/** Indicates the type of the third party contents. */
enum TPTYPE {
	/** unknown content */
	TP_UNKNOWN,
	/** Meteotime weather (encrypted) */
	TP_WEATHER,
	/** German civil warning (unused) */
	TP_ALARM
};

#include <stdint.h>

/**
 * Initialize internal structures.
 */
void init_thirdparty(void);

/**
 * Add the current bit value to the third party buffer.
 *
 * @param minute The current value of the minute.
 * @param bitpos The current bit position.
 * @param bit The current bit value.
 */
void fill_thirdparty_buffer(uint8_t minute, uint8_t bitpos, uint16_t bit);

/**
 * Retrieve the third party buffer.
 *
 * @return The third party buffer.
 */
const uint8_t * const get_thirdparty_buffer(void);

/**
 * Retrieve the type of the third party contents.
 *
 * @return The third party status.
 */
enum TPTYPE get_thirdparty_type(void);

#endif
