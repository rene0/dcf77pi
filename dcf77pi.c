#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sysexits.h>
#include <time.h>

#include "input.h"
#include "decode_time.h"
#include "decode_alarm.h"

int
main(int argc, char *argv[])
{
	uint8_t indata[40];
	uint8_t civbuf[40];
	struct tm time, oldtime;
	uint8_t civ1, civ2;
	int dt, bit, bitpos, minlen = 0, init = 1, init2 = 0;

	if (argc != 2) {
		printf("usage: %s infile\n", argv[0]);
		return EX_USAGE;
	}
	if (strcmp(argv[1], "-") ? set_mode(0, argv[1]) : set_mode(1, NULL)) {
		/* something went wrong */
		cleanup();
		return 0;
	}

	/* no weird values please */
	bzero(indata, sizeof(indata));
	civ1 = civ2 = 2; /* initial invalid values */
	bzero(&time, sizeof(time));

	for (;;) {
		bit = get_bit();
		while (bit & GETBIT_READ)
			bit = get_bit();
		if (bit & GETBIT_EOD)
			break;

		bitpos = get_bitpos();
		if (bit & GETBIT_EOM)
			minlen = bitpos;
		else
			display_bit();

		if (!init) {
			switch (time.tm_min % 3) {
			case 0:
				/* copy civil warning bits */
				if (bitpos == 1)
					civ1 = bit & GETBIT_ONE;
				if (bitpos == 8)
					civ2 = bit & GETBIT_ONE;
				break;
			case 2:
				if (bitpos == 15)
					memcpy(civbuf, indata, sizeof(civbuf));
					/* save civil warning buffer */
				break;
			default:
				break;
			}
		}

		bit = next_bit();
		if (bit & GETBIT_TOOLONG)
			printf(" >");

		if (bit & GETBIT_EOM) {
			dt = decode_time(init2, minlen, get_buffer(), &time);
			printf(" %d %c\n", minlen, dt & DT_LONG ? '>' :
			    dt & DT_SHORT ? '<' : ' ');

			if (civ1 == 1 && civ2 == 1)
				display_alarm(civbuf); /* no real decoding */
			else
				if (civ1 != civ2)
					printf("Civil warning error\n");

			if (!init) {
				dt = add_minute(&oldtime, dt);
				display_time(init2, dt, oldtime, time);
			}
			printf("\n");

			memcpy((void *)&oldtime, (const void *)&time,
			    sizeof(struct tm));
			if (init2)
				init2 = 0;
			if (init) {
				init2 = 1;
				init = 0;
			}
		}
	}

	cleanup();
	return 0;
}
