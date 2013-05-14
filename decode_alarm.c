#include <stdio.h>
#include "decode_alarm.h"

void
display_alarm(uint8_t *buf)
{
	int ds1, ds2, ps1, ps2, dl1, dl2, pl1, pl2;

	ds1 = buf[0] + 2 * buf[1] + 4 * buf[3];
	ps1 = buf[2] + 2 * buf[4] + 4 * buf[5];
	dl1 = buf[12] + 2 * buf[13] + 4 * buf[14] + 8 * buf[15] +
	    16 * buf[16] + 32 * buf[17] + 64 * buf[19] + 128 * buf[20] +
	    256 * buf[21] + 512 * buf[23];
	pl1 = buf[18] + 2 * buf[22] + 4 * buf[24] + 8 * buf[25];

	ds2 = buf[6] + 2 * buf[7] + 4 * buf[9];
	ps2 = buf[8] + 2 * buf[10] + 4 * buf[11];
	dl2 = buf[26] + 2 * buf[27] + 4 * buf[28] + 8 * buf[29] +
	    16 * buf[30] + 32 * buf[31] + 64 * buf[33] + 128 * buf[34] +
	    256 * buf[35] + 512 * buf[37];
	pl2 = buf[32] + 2 * buf[36] + 4 * buf[38] + 8 * buf[39];

	printf("\nCivil warning (Germany): ");
	if (ds1 == ds2 && ps1 == ps2 && dl1 == dl2 && pl1 == pl2)
		printf("short: data=%d parity=0x%x long: data=%d parity=0x%x\n",
		    ds1, ps1, dl1, pl1);
	else /* should not happen */
		printf("differences: %x %x %x %x\n", ds1-ds2, ps1-ps2, dl1-dl2, pl1-pl2);
}
