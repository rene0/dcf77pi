// Copyright 2013-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"
#include "json_util.h"

#include <sys/types.h> /* FreeBSD < 12.0 */
#include <sys/event.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* for strerror() */
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

static struct json_object *config;

int main(void)
{
	struct kevent change;	 /* event we want to monitor */
	struct kevent event;	  /* event that was triggered */
	struct timespec ts, oldts;
	struct hardware hw;
	int kq, count, res;
	long long diff, corr, diffsum;
	bool first_time = true;

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
		exit(EX_NOINPUT);
	}
	hw = get_hardware_parameters();

	/* create a new kernel event queue */
	if ((kq = kqueue()) == -1) {
		perror("kqueue()");
		exit(EX_UNAVAILABLE);
	}

	clock_getres(CLOCK_MONOTONIC, &ts);
	printf("hw.freq=%u resolution=%li:%li\n", hw.freq, ts.tv_sec, ts.tv_nsec);
	/* initalise kevent structure */
	/* timing is rather sloppy, sometimes 6 ms (!) late and skews behind (~0.8s/hour) */
	/* also drifts over time, but correction is overdone? */
	EV_SET(&change, 1, EVFILT_TIMER, EV_ADD | EV_ENABLE, NOTE_USECONDS, 1000000 / hw.freq, 0);

	count = corr = diffsum = 0;
	/* loop forever */
	for (;;) {
		int nev = kevent(kq, &change, 1, &event, 1, NULL);

		if (nev < 0) {
			perror("kevent()");
			exit(EX_UNAVAILABLE);
		}

		else if (nev > 0) {
			if (event.flags & EV_ERROR) {	/* report any error */
				fprintf(stderr, "EV_ERROR: %s\n", strerror(event.data));
				exit(EX_UNAVAILABLE);
			}
			count++;
			printf("%i", get_pulse());
			if (count % (hw.freq /*- corr*/) == 0) {
				clock_gettime(CLOCK_MONOTONIC, &ts);
				printf(" count=%li:%i", event.data, count);
				if (first_time) {
					first_time = false;
				} else {
					diff = ((ts.tv_sec * 1000000000 + ts.tv_nsec) - ((oldts.tv_sec + 1) * 1000000000 + oldts.tv_nsec)); /* add 1 second to old time for predicted value, in ns */
					corr = hw.freq * diff / 1000000000;
					diffsum += diff;
					printf(" ns diff=%lli corr=%lli diffsum=%lli", diff, corr, diffsum);
				}
				printf("\n");
				count = 0;
				memcpy(&oldts, &ts, sizeof(struct timespec));
			}
		}
	}

	close(kq);
	cleanup();
	free(config);
	return EX_OK;
}
