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
void decode_alarm(const unsigned civbuf[], struct alm * const alarm);

/**
 * Determines the name of the region which the alarm is broadcasted for.
 *
 * @param alarm The structure containing the alarm information.
 * @return The region name.
 */
const char * const get_region_name(struct alm alarm);

#endif
