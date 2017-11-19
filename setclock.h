/*
Copyright (c) 2013-2014, 2016-2017 René Ladan. All rights reserved.

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
struct DT_result;
struct GB_result;
struct tm;

/** State for setting the clock */
enum eSC_status {
	/** Clock was set successfully */
	esc_ok,
	/** Time was invalid */
	esc_invalid,
	/** Settting the clock failed */
	esc_fail,
	/** Too early or unsafe to set the time */
	esc_unsafe
};

/**
 * Check if it is OK to set the system clock.
 *
 * @param init_min Indicates whether the state of the decoder is initial
 * @param dt The status of the currently decoded time
 * @param bit The current bit information
 * @return Whether it is OK to set the system clock
 */
bool setclock_ok(unsigned init_min, struct DT_result dt, struct GB_result bit);

/**
 * Set the system clock according to the given time.
 *
 * @param settime The time to set the system clock to, in ISO or DCF77 format.
 * @return Whether the clock was set successfully.
 */
enum eSC_status setclock(struct tm settime);

#endif
