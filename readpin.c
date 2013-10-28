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

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "input.h"

int
main(int argc, char **argv)
{
	int i, act, pas, minlimit, maxlimit, sec, init;
	uint8_t p, p0;
	struct hardware *hw;
	struct timespec tp0, tp1, slp;
	long twait;
	long long diff;

	if (set_mode(0, NULL, NULL)) {
		cleanup();
		return 0;
	}
	hw = get_hardware_parameters();

	p0 = 255;
	minlimit = hw->freq * hw->min_len / 100;
	maxlimit = hw->freq * hw->max_len / 100;
	sec = -1;
	init = 1;
	diff = 0;
	printf("limit=[%i..%i]\n", minlimit, maxlimit);

	for (i = act = pas = 0;; i++) {
		if (clock_gettime(CLOCK_MONOTONIC, &tp0) != 0) {
			perror("before pulse");
			break;
		}
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
			if (p0 == 0) {
				/* theoretically a new second */
				if (i > minlimit && (init || i < maxlimit)) {
					printf(" (%i %i %i) %i %lli %lli === %i\n", act, pas, i, act*100/i, diff, diff / i / 1000, ++sec);
					/* new second */
					i = act = pas = 0;
					init = 0;
					diff = 0;
				} else if (i > minlimit * 2 && (init ||
				    i < maxlimit * 2)) {
					printf(" (%i %i %i) %i %lli %lli $$$ %i\n", act, pas, i, act*100/i, diff, diff / i / 1000, ++sec);
					sec = -1; /* new second and new minute */
					i = act = pas = 0;
					init = 0;
					diff = 0;
				} else if (init) {
					/* end of partial second? */
					printf(" (%i %i %i) %i <<<\n", act, pas, i, act*100/i);
					i = act = pas = 0;
					init = 0;
					diff = 0;
				}
				if (i > maxlimit/5) {
					printf("M");
					act--;
					pas++;
				}
			}
			if (p0 == 1) {
				if (i < maxlimit/10) {
					printf("P");
					act++;
					pas--;
				}
			}
		}
		if (i > maxlimit * 2) {
			printf(" {%i %i %i} %i >>>\n", act, pas, i, act*100/i); /* timeout */
			i = act = pas = 0;
		}
		p0 = p;
		if (clock_gettime(CLOCK_MONOTONIC, &tp1) != 0) {
			perror("before sleep");
			break;
		}
		twait = 1e9 / hw->freq - (tp1.tv_sec - tp0.tv_sec) * 1e9 - (tp1.tv_nsec - tp0.tv_nsec);
		if (twait > 0) { /* 1000 Hz, 80..105 -> -713 us seen ... */
			slp.tv_sec = twait / 1e9;
			slp.tv_nsec = twait % 1000000000;
			while (nanosleep(&slp, &slp))
				;
			if (clock_gettime(CLOCK_MONOTONIC, &tp0) != 0) {
				perror("after sleep");
				break;
			}
			twait = (tp0.tv_sec - tp1.tv_sec) * 1e9 + (tp0.tv_nsec - tp1.tv_nsec) - twait;
			diff += twait;
		}
	}
	cleanup();
	return 0;
}
