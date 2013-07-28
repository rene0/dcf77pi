/*
Copyright (c) 2013 René Ladan. All rights reserved.

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

#include <stdio.h>
#include <unistd.h>
#include "input.h"

int
main(int argc, char **argv)
{
	int i, p, p0, act, pas, minlimit, maxlimit, count, sec, init;
	struct hardware hw;

	if (set_mode(0, NULL, NULL)) {
		cleanup();
		return 0;
	}

	(void)read_hardware_parameters(ETCDIR"/hardware.txt", &hw);
	/* get our own copy, error handling in set_mode() */

	p0 = -1;
	minlimit = hw.freq * hw.min_len / 100;
	maxlimit = hw.freq * hw.max_len / 100;
	init = 1;
	printf("limit=[%i..%i]\n", minlimit, maxlimit);

	for (i = act = pas = 0;; i++) {
		p = get_pulse();
		if (p & GETBIT_IO) {
			printf("IO error!\n");
			break;
		}
		if (p == 0) {
			pas++;
			//printf("-");
		} else { /* p == 1 */
			act++;
			if (p0 == 0) {
				count = act * 100/i;
				if (i > minlimit * 2 && i < maxlimit * 2)
					count *= 2;
				printf(" (%i %i %i) %i ", act, pas, i, count);
				if (i > minlimit && (init || i < maxlimit)) {
					i = act = pas = 0;
					init = 0;
				} else if (i > minlimit * 2 && (init || i < maxlimit * 2)) {
					i = act = pas = 0;
					init = 0;
				} else {
					if (count > 95) /* maybe parametrize */
						printf("P"); /* act already increased */
					else if (init) {
						/* end of partial second? */
						printf(" (%i %i %i) <<<\n", act, pas, i);
						i = act = pas = 0;
						init = 0;
					} else {
						printf("M");
						act--;
						pas++;
					}
				}
			}
			//printf("+");
		}
		if (i > maxlimit * 2) {
			printf(" (%i %i %i) >>>\n", act, pas, i); /* timeout */
			i = act = pas = 0;
		}
		p0 = p;
		(void)usleep(1000000.0 / hw.freq);
	}
	cleanup();
	return 0;
}
