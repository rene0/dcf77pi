// Copyright 2017 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "bits1to14.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

int
main(int argc, char *argv[])
{
	unsigned in_buf[TPBUFLEN];
	const unsigned *rb_ptr;
	enum eTP tp_res[4] = { eTP_weather, eTP_unknown, eTP_unknown,
	    eTP_alarm };
	struct GB_result gbr;

	srand(1); /* INSECURE random function, but C99-compliant */
	for (int ofs = 0; ofs < 3; ofs++) {
		for (unsigned type = 0; type < 4; type++) {
			/* fill the buffer */
			for (int i = 0; i < TPBUFLEN + 2; i++) {
				/*
				 * Calculate absolute position in buffer when
				 * jumping in half-way a message.
				 */
				int k = i - 14 * ofs;
				if (k < 0)
					k += TPBUFLEN + 2;
				if (k == 0) {
					gbr.bitval = ((type & 1) == 1 ? ebv_1 :
					    ebv_0);
				} else if (k == 7) {
					gbr.bitval = ((type & 2) == 2 ? ebv_1 :
					    ebv_0);
				} else {
					int j = (k < 7) ? k - 1 : k - 2;
					in_buf[j] = rand() % 2;
					gbr.bitval = (in_buf[j] == 0 ? ebv_0 :
					    ebv_1);
				}
				fill_thirdparty_buffer(k / 14, k % 14 + 1, gbr);
			}

			/* test retrieving the buffer */
			rb_ptr = get_thirdparty_buffer();
			for (unsigned i = 0; i < TPBUFLEN; i++) {
				if (in_buf[i] != rb_ptr[i]) {
					printf("%s: ofs %i type %u bit %u:"
					   " %u must be %u\n", argv[0], ofs,
					   type, i, rb_ptr[i], in_buf[i]);
					return EX_SOFTWARE;
				}
			}

			/* test retrieving the type */
			enum eTP tp_type;
			tp_type = get_thirdparty_type();
			if (tp_type != tp_res[type]) {
				printf("%s: ofs %i: type %i must be %i\n",
				    argv[0], ofs, tp_type, tp_res[type]);
				return EX_SOFTWARE;
			}
		}
	}
	return EX_OK;
}
