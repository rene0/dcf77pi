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

#include "decode_alarm.h"

#include "input.h"

uint8_t civbuf[CIVBUFLEN];
uint8_t civstat;

void
decode_alarm(struct alm *alarm)
{
	alarm->ds1 = civbuf[0] + 2 * civbuf[1] + 4 * civbuf[3];
	alarm->ps1 = civbuf[2] + 2 * civbuf[4] + 4 * civbuf[5];
	alarm->dl1 = civbuf[12] + 2 * civbuf[13] + 4 * civbuf[14] +
	    8 * civbuf[15] + 16 * civbuf[16] + 32 * civbuf[17] +
	    64 * civbuf[19] + 128 * civbuf[20] + 256 * civbuf[21] +
	    512 * civbuf[23];
	alarm->pl1 = civbuf[18] + 2 * civbuf[22] + 4 * civbuf[24] +
	    8 * civbuf[25];

	alarm->ds2 = civbuf[6] + 2 * civbuf[7] + 4 * civbuf[9];
	alarm->ps2 = civbuf[8] + 2 * civbuf[10] + 4 * civbuf[11];
	alarm->dl2 = civbuf[26] + 2 * civbuf[27] + 4 * civbuf[28] +
	    8 * civbuf[29] + 16 * civbuf[30] + 32 * civbuf[31] +
	    64 * civbuf[33] + 128 * civbuf[34] + 256 * civbuf[35] +
	    512 * civbuf[37];
	alarm->pl2 = civbuf[32] + 2 * civbuf[36] + 4 * civbuf[38] +
	    8 * civbuf[39];
}

void
fill_civil_buffer(int minute, int bitpos, uint16_t bit)
{
	switch (minute % 3) {
	case 0:
		/* copy civil warning data */
		if (bitpos > 1 && bitpos < 8)
			civbuf[bitpos - 2] = bit & GETBIT_ONE;
			/* 2..7 -> 0..5 */
		if (bitpos > 8 && bitpos < 15)
			civbuf[bitpos - 3] = bit & GETBIT_ONE;
			/* 9..14 -> 6..11 */

		/* copy civil warning flags */
		if (bitpos == 1)
			civstat = (bit & GETBIT_ONE) << 1;
		if (bitpos == 8)
			civstat |= bit & GETBIT_ONE;
		break;
	case 1:
		/* copy civil warning data */
		if (bitpos > 0 && bitpos < 15)
			civbuf[bitpos + 11] = bit & GETBIT_ONE;
			/* 1..14 -> 12..25 */
		break;
	case 2:
		/* copy civil warning data */
		if (bitpos > 0 && bitpos < 15)
			civbuf[bitpos + 25] = bit & GETBIT_ONE;
			/* 1..14 -> 26..39 */
		break;
	}
{

}
