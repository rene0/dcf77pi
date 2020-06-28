// Copyright 2013-2018 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

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
 * @param gbr The current bit information
 * @return Whether it is OK to set the system clock
 */
bool setclock_ok(unsigned init_min, struct DT_result dt, struct GB_result gbr);

/**
 * Set the system clock according to the given time.
 *
 * @param settime The time to set the system clock to, in ISO or DCF77 format.
 * @return Whether the clock was set successfully.
 */
enum eSC_status setclock(struct tm settime);

#endif
