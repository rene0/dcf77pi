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
	int i, p, p0, act, pas, minlimit, maxlimit, sec, init;
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
	sec = -1;
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
			printf("-");
		}
		if (p == 1) {
			act++;
			printf("+");
		}
		if (p0 != p) {
			printf(" (%i %i %i) %i ", act, pas, i, act*100/i);
			if (p0 == 0) {
				/* theoretically a new second */
				if (i > minlimit && (init || i < maxlimit)) {
					printf(" === %i\n", ++sec);
					/* new second */
					i = act = pas = 0;
					init = 0;
				} else if (i > minlimit * 2 && (init ||
				    i < maxlimit * 2)) {
					printf(" $$$ %i\n", ++sec);
					sec = -1; /* new second and new minute */
					i = act = pas = 0;
					init = 0;
				} else if (init) {
					/* end of partial second? */
					printf(" <<<\n");
					i = act = pas = 0;
					init = 0;
				}
				if (i > maxlimit/5) {
					printf("M");
					act--;
					pas++;
				} else
					printf("/");
			}
			if (p0 == 1) {
				if (i < maxlimit/10) {
					printf("P");
					act++;
					pas--;
				} else
					printf("\\");
			}
		}
		if (i > maxlimit * 2) {
			printf(" {%i %i %i} %i >>>\n", act, pas, i, act*100/i); /* timeout */
			i = act = pas = 0;
		}
		p0 = p;
		(void)usleep(1000000.0 / hw.freq);
	}
	cleanup();
	return 0;
}
