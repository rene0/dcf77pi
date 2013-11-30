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

/*
 * Idea and partial implementation for the exponential low-pass filter and
 * Schmitt trigger taken from Udo Klein, with permisssion.
 * http://blog.blinkenlight.net/experiments/dcf77/binary-clock/#comment-5916
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "config.h"
#include "input.h"

int
main(int argc, char **argv)
{
	int t, tlow, thigh, sec, init, res, stv;
	uint8_t p;
	struct hardware *hw;
#ifdef TUNETIME
	struct timespec tp0, tp1;
	long long diff = 0;
#endif
	struct timespec slp;
	long twait;
	float a, y;

	res = read_config_file(ETCDIR"/config.txt");
	if (res != 0) {
		cleanup();
		return res;
	}
	res = set_mode(0, NULL, NULL);
	if (res != 0) {
		cleanup();
		return res;
	}
	hw = get_hardware_parameters();

	sec = -1;
	init = 1;
	tlow = thigh = 0;
	stv = -1;

	/* Set up filter, reach 50% after realfreq/20 samples (i.e. 50 ms) */
	a = 1.0 - exp2(-1.0 / (hw->realfreq / 20.0));
	y = -1;
	printf("realfreq=%lu filter_a=%f\n", hw->realfreq, a);

	for (t = 0;; t++) {
#ifdef TUNETIME
		if (clock_gettime(CLOCK_MONOTONIC, &tp0) != 0) {
			perror("before pulse");
			break;
		}
#endif
		p = get_pulse();
		if (p & GETBIT_IO) {
			printf("IO error!\n");
			break;
		}
		y = y < 0 ? (float)p : y + a * (p - y);
		if (stv == -1)
			stv = p;
		printf("%c", p == 0 ? '-' : p == 1 ? '+' : '?');

		/* Schmitt trigger, maximize value to introduce hysteresis and avoid infinite memory */
		if (y < 0.5 && stv == 1) {
			y = 0.0;
			stv = 0;
			tlow = t;
		}
		if (y > 0.5 && stv == 0) {
			y = 1.0;
			stv = 1;
			thigh = t;
			if (init == 1)
				init = 0;
			else
				sec++;
			//TODO detect timeouts, other errors, bit value
			printf(" (%u %u) %i", tlow, thigh, sec);
#ifdef TUNETIME
			printf(" %lli", diff);
			diff = 0;
#endif
			printf("\n");
			t = 0;
			if (thigh > hw->freq * 3/2) {
				sec = -1;
			}
		}
#ifdef TUNETIME
		if (clock_gettime(CLOCK_MONOTONIC, &tp1) != 0) {
			perror("before sleep");
			break;
		}
#endif
		twait = 1e9 / hw->freq;
#ifdef TUNETIME
		twait -= (tp1.tv_sec - tp0.tv_sec) * 1e9 - (tp1.tv_nsec - tp0.tv_nsec);
#endif
		if (twait <= 0)
			/* 1000 Hz -> -713 us seen */
			printf(" <%li> ", twait);
		else {
			slp.tv_sec = twait / 1e9;
			slp.tv_nsec = twait % 1000000000;
			while (nanosleep(&slp, &slp))
				;
#ifdef TUNETIME
			if (clock_gettime(CLOCK_MONOTONIC, &tp0) != 0) {
				perror("after sleep");
				break;
			}
			twait = (tp0.tv_sec - tp1.tv_sec) * 1e9 + (tp0.tv_nsec - tp1.tv_nsec) - twait;
			diff += twait;
#endif
		}
	}
	cleanup();
	return 0;
}
