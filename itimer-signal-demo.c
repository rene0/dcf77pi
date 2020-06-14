// Copyright 2013-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"
#include "json_object.h"
#include "json_util.h"

#include <sys/types.h> /* FreeBSD < 12.0 */
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

int
main(void)
{
	struct hardware hw;
	struct json_object *config;
	struct itimerval itv;
	sigset_t myset;
	long long interval;
	int oldpulse, count, second, res, act, pas, minute, pulse, bump_second;
	int dummy; /* for sigwait */
	bool change_interval, synced;

	config = json_object_from_file(ETCDIR "/config.json");
	if (config == NULL) {
		cleanup();
		free(config);
		exit(EX_NOINPUT);
	}
	res = set_mode_live(config);
	if (res != 0) {
		cleanup();
		free(config);
		exit(res);
	}
	hw = get_hardware_parameters();

	change_interval = false;
	interval = 1e6 / hw.freq; /* initial value */

	/* set up the timer */
	itv.it_interval.tv_sec = itv.it_value.tv_sec = 0;
	itv.it_interval.tv_usec = itv.it_value.tv_usec = interval;
	(void)setitimer(ITIMER_REAL, &itv, NULL);

	(void)sigemptyset(&myset);
	if (sigaddset(&myset, SIGALRM)) {
		perror("Failed adding SIGALRM\n");
		return errno;
	}
	if (sigprocmask(SIG_BLOCK, &myset, NULL)) {
		perror("Failed sigprocmask\n");
		return errno;
	}

	pulse = 3; /* nothing yet */
	count = 0; /* invariant : count == act+pas ? */
	second = 0;
	minute = -1;
	synced = false;
	act = pas = 0;
	oldpulse = -1;
	bump_second = 0;
	printf("%i:%i\n", minute, second);
	/* loop forever */
	for (;;) {
		pulse = get_pulse();
		if (pulse == 2) {
			printf("*\n");
			continue;
		}
		if (count >= hw.freq) {
			if (act > 0 && pas == 0) {
				interval--;
				printf("< ");
				change_interval = true;
				count = act;
			} else {
				count = 0;
			}
			if (pas > 0 && pas < 2 * hw.freq) {
				interval++;
				printf("> ");
				change_interval = true;
			}
			printf("C %i %i\n", act, pas);
			if (act >= 2 * hw.freq || pas >= 2 * hw.freq) {
				// no radio signal
				act = pas = 0;
				bump_second = 2;
				printf("N ");
			}
		}
		if (pulse == 1) {
			act++;
		} else {
			pas++;
		}
		if (oldpulse == 0 && pulse == 1) {
			// this now assumes a clean signal without a sw filter!
			if (act + pas > 0.8 * hw.freq) {
			//if (act + pas == hw.freq || act + pas == 2 * hw.freq)
				// start of new second
				bump_second = 1;
				if (!synced) {
					// first second
					synced = true;
					count = 0;
					printf("R ");
				}
				printf("P %i %i %i\n", act, pas, count);
				if (count > 0 && hw.freq / 2 > count) {
					interval++;
					change_interval = true;
					printf("+ ");
				}
				if (hw.freq / 2 < count) {
					interval--;
					change_interval = true;
					printf("- ");
				}
			} else if (synced) {
				// double-pulse NPL (or bad reception)
				printf("X %i %i %i ", act, pas, count);
			}
			if (pas > 1.5 * hw.freq) { // DCF77
				bump_second = 0;
				second = 0;
				minute++;
				printf("%i:%i\n", minute, second);
			}
			if (act + pas > 0.8 * hw.freq) {
			//if (act + pas == hw.freq || act + pas == 2 * hw.freq)
				/* reset here instead of above because of the act/pas minute tests */
				act = pas = 0;
			}
		}
		if (interval < 8e5 / hw.freq || interval > 1.2e6 / hw.freq) {
			printf("# %lli -> %f\n", interval, 1e6 / hw.freq);
			interval = 1e6 / hw.freq;
		}
		if (change_interval) {
			change_interval = false;
			itv.it_interval.tv_sec = 0;
			itv.it_interval.tv_usec = interval;
			(void)setitimer(ITIMER_REAL, &itv, NULL);
			printf("I %lli\n", interval);
		}
		if (bump_second != 0) {
			second += bump_second;
			if (second > 60) {
				second = 0;
			}
			bump_second = 0;
			printf("%i:%i\n", minute, second);
		}
		oldpulse = pulse;
		count++;
		(void)sigwait(&myset, &dummy);
	}

	cleanup();
	free(config);
	return EX_OK;
}
