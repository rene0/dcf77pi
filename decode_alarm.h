// Copyright 2013-2018 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DCF77PI_DECODE_ALARM_H
#define DCF77PI_DECODE_ALARM_H

/**
 * Structure for the (defunct) German civil warning system
 *
 * confirmed by "Vortrag INS Bevoelkerungswarnung Hannovermesse 2008.pdf"
 * (c) 2008 DIN e.V.
 * With permission from Mr. Karl Wenzelewski of DIN
 */
struct alm {
	struct {
		unsigned r1, r2, r3, r4;
	} region[2];
	struct {
		unsigned ps, pl;
	} parity[2];
};

/**
 * Decode the alarm buffer into the various fields of "struct alm"
 *
 * @param civbuf The input buffer containing the civil alarm.
 * @param alarm The structure containing the decoded values.
 */
void decode_alarm(const unsigned civbuf[], struct alm * alarm);

/**
 * Determines the name of the region which the alarm is broadcasted for.
 *
 * @param alarm The structure containing the alarm information.
 * @return The region name.
 */
const char * get_region_name(struct alm alarm);

#endif
