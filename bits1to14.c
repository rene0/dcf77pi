// Copyright 2014-2016 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "bits1to14.h"

#include "input.h"

static unsigned tpbuf[TPBUFLEN];
static enum eTP tptype = eTP_unknown;

void
fill_thirdparty_buffer(int minute, int bitpos, struct GB_result bit)
{
	static unsigned tpstat;

	switch (minute % 3) {
	case 0:
		/* copy third party data */
		if (bitpos > 1 && bitpos < 8)
			tpbuf[bitpos - 2] = bit.bitval == ebv_1 ? 1 : 0;
			/* 2..7 -> 0..5 */
		if (bitpos > 8 && bitpos < 15)
			tpbuf[bitpos - 3] = bit.bitval == ebv_1 ? 1 : 0;
			/* 9..14 -> 6..11 */

		/* copy third party type */
		if (bitpos == 1)
			tpstat = bit.bitval == ebv_1 ? 2 : 0;
		if (bitpos == 8) {
			if (bit.bitval == ebv_1)
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
			tpbuf[bitpos + 11] = bit.bitval == ebv_1 ? 1 : 0;
			/* 1..14 -> 12..25 */
		break;
	case 2:
		/* copy third party data */
		if (bitpos > 0 && bitpos < 15)
			tpbuf[bitpos + 25] = bit.bitval == ebv_1 ? 1 : 0;
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
