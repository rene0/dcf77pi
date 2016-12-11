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

#ifndef DCF77PI_SETCLOCK_H
#define DCF77PI_SETCLOCK_H

#include <stdbool.h>
#include <stdint.h>
struct DT_result;
struct GB_result;
struct tm;

/**
 * Check if it is OK to set the system clock.
 *
 * @param init_min Indicates whether the state of the decoder is initial
 * @param dt The status of the currently decoded time
 * @param bit The current bit information
 * @return Whether it is OK to set the system clock
 */
bool setclock_ok(uint8_t init_min, const struct DT_result * const dt,
    const struct GB_result * const bit);

/**
 * Set the system clock according to the given time. Note that this does *not*
 * work with no timezone or UTC.
 *
 * @param time The time to set the system clock to.
 * @return The clock was set successfully (0), or the time was invalid (-1), or
 *   setting the clock somehow failed (-2).
 */
int8_t setclock(struct tm time);

#endif
