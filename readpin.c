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

/*
 * Idea and partial implementation for the exponential low-pass filter and
 * Schmitt trigger taken from Udo Klein, with permisssion.
 * http://blog.blinkenlight.net/experiments/dcf77/binary-clock/#comment-5916
 */
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>
#include <time.h>
#include "config.h"
#include "input.h"

int
main(int argc, char **argv)
{
	int t, tlow, sec, res, opt, tunetime = 0;
	uint8_t p, bit, init, stv, newminute;
	struct hardware *hw;
	struct timespec tp0, tp1;
	long long diff = 0;
	struct timespec slp;
	long twait;
	float a, y, w, realfreq;

	while ((opt = getopt(argc, argv, "t")) != -1) {
		switch (opt) {
		case 't' :
			tunetime = 1;
			break;
		default:
			printf("usage: %s [-t]\n", argv[0]);
			return EX_USAGE;
		}
	}

	w = 0.05;
	res = read_config_file(ETCDIR"/config.txt");
	if (res != 0) {
		cleanup();
		return res;
	}
	res = set_mode(NULL, NULL);
	if (res != 0) {
		cleanup();
		return res;
	}
	hw = get_hardware_parameters();

	sec = -1;
	init = 1;
	tlow = 0;
	stv = 2;

	realfreq = hw->freq; /* initial value */

	/* Set up filter, reach 50% after realfreq/20 samples (i.e. 50 ms) */
	a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
	y = -1;
	printf("realfreq=%f filter_a=%f\n", realfreq, a);

	for (t = 0;; t++) {
		if (tunetime == 1 && clock_gettime(CLOCK_MONOTONIC, &tp0) != 0) {
			perror("before pulse");
			break;
		}
		p = get_pulse();
		if (p & GETBIT_IO) {
			printf("IO error!\n");
			break;
		}
		y = y < 0 ? (float)p : y + a * (p - y);
		if (stv == 2)
			stv = p;
		printf("%c", p == 0 ? '-' : p == 1 ? '+' : '?');

		if (realfreq < hw->freq / 2) {
			printf("<");
			realfreq = hw->freq;
		}
		if (realfreq > hw->freq * 3/2) {
			printf(">");
			realfreq = hw->freq;
		}

		if (t > realfreq * 5/2) {
			realfreq = realfreq + w * ((t/2.5) - realfreq);
			a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
			printf(" {%4u %4u} %3i %f %f", tlow, t, sec, realfreq, a);
			t = 0; /* timeout */
			if (tunetime == 1) {
				printf(" %lli", diff);
				diff = 0;
			}
			printf("\n");
		}
		/*
		 * Schmitt trigger, minimize/maximize value of y to introduce
		 * hysteresis and avoid infinite memory
		 * */
		if (y < 0.5 && stv == 1) {
			y = 0.0;
			stv = 0;
			tlow = t;
		}
		if (y > 0.5 && stv == 0) {
			y = 1.0;
			stv = 1;
			newminute = t > realfreq * 3/2;
			if (init == 1)
				init = 0;
			else {
				sec++;
				if (newminute)
					realfreq = realfreq + w * ((t/2) - realfreq);
				else
					realfreq = realfreq + w * (t - realfreq);
				/* adjust filter */
				a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
			}
			res = tlow * 100 / t;
			if (newminute)
				res *= 2;
			/* in general, pulses are a bit wider than specified */
			if (res <= hw->maxzero)
				bit = 0;
			else if (res <= hw->maxone)
				bit = 1;
			else
				bit = 2; /* some error */
			printf(" (%4u %4u) %u %3i %3i %f %f", tlow, t, bit, res, sec, realfreq, a);
			if (tunetime == 1) {
				printf(" %lli", diff);
				diff = 0;
			}
			printf("\n");
			if (newminute)
				sec = -1; /* new minute */
			t = 0;
		}
		if (tunetime == 1 && clock_gettime(CLOCK_MONOTONIC, &tp1) != 0) {
			perror("before sleep");
			break;
		}
		twait = 1e9 / hw->freq;
		if (tunetime == 1)
			twait -= (tp1.tv_sec - tp0.tv_sec) * 1e9 - (tp1.tv_nsec - tp0.tv_nsec);
		if (twait <= 0)
			/* 1000 Hz -> -713 us seen */
			printf(" <%li> ", twait);
		else {
			slp.tv_sec = twait / 1e9;
			slp.tv_nsec = twait % 1000000000; /* clang 3.3 does not like 1e9 here */
			while (nanosleep(&slp, &slp))
				;
			if (tunetime == 1 && clock_gettime(CLOCK_MONOTONIC, &tp0) != 0) {
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
