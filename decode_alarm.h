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

#ifndef DCF77PI_DECODE_ALARM_H
#define DCF77PI_DECODE_ALARM_H

#include <stdint.h>

/* Length of the civil warning buffer in bits */
#define CIVBUFLEN 40

/*
 * From German wikipedia mostly, long regions and parities are unspecified
 * short regions: 1=north, 2=middle, 4=south
 * xx1 and xx2 must be equal
 * ds = short region
 * dl = long region
 * ps = parity over ds
 * pl = parity over dl
 */
struct alm {
	uint8_t ds1, ds2, ps1, ps2, pl1, pl2;
	uint16_t dl1, dl2;
};

/**
 * Initialize internal structures.
 */
void init_alarm(void);

/**
 * Decode the alarm buffer into the various fields of "struct alm"
 *
 * @param alarm The structure containing the decoded values.
 */
void decode_alarm(struct alm *alarm);

/**
 * Add the current bit value to the alarm buffer.
 *
 * @param minute The current value of the minute.
 * @param bitpos The current bit position.
 * @param bit The current bit value.
 */
void fill_civil_buffer(int minute, int bitpos, uint16_t bit);

/**
 * Retrieve the alarm buffer (e.g. for display purposes).
 *
 * @return The alarm buffer.
 */
uint8_t *get_civil_buffer(void);

/**
 * Retrieve the value of the alarm status:
 * 0   = no alarm
 * 1,2 = inconsitent state
 * 3   = alarm
 *
 * @return The alarm status.
 */
uint8_t get_civil_status(void);

#endif
