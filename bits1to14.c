/*
Copyright (c) 2014, 2016 René Ladan. All rights reserved.

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

#include "bits1to14.h"

#include "input.h"

const unsigned tpBufLen = 40;
static unsigned tpbuf[tpBufLen];
static enum eTP tptype = eTP_unknown;

void
fill_thirdparty_buffer(unsigned minute, unsigned bitpos,
    const struct GB_result * const bit)
{
	static unsigned tpstat;

	switch (minute % 3) {
	case 0:
		/* copy third party data */
		if (bitpos > 1 && bitpos < 8)
			tpbuf[bitpos - 2] = bit->bitval == ebv_1 ? 1 : 0;
			/* 2..7 -> 0..5 */
		if (bitpos > 8 && bitpos < 15)
			tpbuf[bitpos - 3] = bit->bitval == ebv_1 ? 1 : 0;
			/* 9..14 -> 6..11 */

		/* copy third party type */
		if (bitpos == 1)
			tpstat = bit->bitval == ebv_1 ? 2 : 0;
		if (bitpos == 8) {
			if (bit->bitval == ebv_1)
				tpstat++;
			switch (tpstat) {
			case 0:
				tptype = eTP_weather;
				break;
			case 3:
				tptype = eTP_alarm;
				break;
			default:
				tptype = eTP_unknown;
				break;
			}
		}
		break;
	case 1:
		/* copy third party data */
		if (bitpos > 0 && bitpos < 15)
			tpbuf[bitpos + 11] = bit->bitval == ebv_1 ? 1 : 0;
			/* 1..14 -> 12..25 */
		break;
	case 2:
		/* copy third party data */
		if (bitpos > 0 && bitpos < 15)
			tpbuf[bitpos + 25] = bit->bitval == ebv_1 ? 1 : 0;
			/* 1..14 -> 26..39 */
		break;
	}
}

const unsigned * const
get_thirdparty_buffer(void)
{
	return tpbuf;
}

enum eTP
get_thirdparty_type(void)
{
	return tptype;
}
