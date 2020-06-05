// Copyright 2013-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"
#include "json_object.h"
#include "json_util.h"

#include <sys/types.h> /* FreeBSD < 12.0 */
#include <sys/gpio.h>
#include <sys/ioccom.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* for strerror() */
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

struct hardware hw;
int fd;
volatile sig_atomic_t wakenup = 0;

/* like set_mode_live() */
int
init_live(struct json_object *config)
{
	struct gpio_pin pin;
	struct json_object *value;

	if (json_object_object_get_ex(config, "pin", &value)) {
		hw.pin = (unsigned)json_object_get_int(value);
	} else {
		printf("Key 'pin' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	if (json_object_object_get_ex(config, "freq", &value)) {
		hw.freq = (unsigned)json_object_get_int(value);
	} else {
		printf("Key 'freq' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	if (json_object_object_get_ex(config, "activehigh", &value)) {
		hw.active_high = (unsigned)json_object_get_boolean(value);
	} else {
		printf("Key 'activehigh' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	fd = open("/dev/gpioc0", O_RDWR);
	if (fd < 0) {
		printf("open /dev/gpioc0 failed\n");
		perror(NULL);
		cleanup();
		return errno;
	}
	pin.gp_pin = hw.pin;
	pin.gp_flags = GPIO_PIN_INPUT;
	if (ioctl(fd, GPIOSETCONFIG, &pin) < 0) {
		perror("ioctl");
		cleanup();
		return errno;
	}
	return 0;
}

void
sigalrm_handler(/*union sigval sv*/ int sig)
{
	wakenup = 1;
}

int
main(void)
{
	struct json_object *config;
#ifdef MODERN_API
	struct sigevent evp;
	struct itimerspec its;
	timer_t timerId;
#endif
	long long interval;
	int oldpulse, count, second, res, act, pas, minute, pulse, bump_second;
	struct gpio_req req;
	bool change_interval, synced;

	config = json_object_from_file(ETCDIR "/config.json");
	if (config == NULL) {
		cleanup();
		free(config);
		exit(EX_NOINPUT);
	}
	res = init_live(config);
	if (res != 0) {
		cleanup();
		free(config);
		exit(res);
	}

#ifdef MODERN_API
	memset((void*)&evp, 0, sizeof evp);
	evp.sigev_notify = SIGEV_THREAD; // TODO what other options are there?
	evp.sigev_notify_function = &sigalrm_handler;
	evp.sigev_signo = SIGALRM; // TODO other signal?
	//evp.sigev_value.sigval_ptr = (void*)this;
	its.it_value.tv_sec = its.it_interval.tv_sec = 0;
	its.it_value.tv_nsec = its.it_interval.tv_nsec = 1e9 / hw.freq;
	res = timer_create(CLOCK_REALTIME, &evp, &timerId);
	if (timer_settime(timerId, 0, &its, NULL) == -1) {
		cleanup();
		free(config);
		printf("timer_settime failed\n");
		exit(EX_OK);
	}
#else
	/* set up the signal handler */
	struct sigaction sigact;
	sigact.sa_handler = sigalrm_handler;
	(void)sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGALRM, &sigact, (struct sigaction *)NULL);
	/* set up the timer */
	struct itimerval itv;
	itv.it_interval.tv_sec = 0;
	change_interval = false;
	interval = 1e6 / hw.freq; // initial value
	itv.it_interval.tv_usec = interval;
	memcpy(&itv.it_value, &itv.it_interval, sizeof(struct timeval));
	(void)setitimer(ITIMER_REAL, &itv, NULL);
#endif
	req.gp_pin = hw.pin;

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
		if (wakenup == 0) {
			//printf("@");
			continue; // should not happen if we sleep forever with sigalrm interruption?
		}
		wakenup = 0;
#ifdef MODERN_API
		printf("(%i)", timer_getoverrun(timerId));
#endif
		res = ioctl(fd, GPIOGET, &req);
		if (res == -1) {
			printf("*\n");
			continue;
		}
		pulse = (req.gp_value == GPIO_PIN_HIGH) ? 1 : 0;
		if (!hw.active_high) {
			pulse = 1 - pulse;
		}
		if (count == hw.freq) {
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
#ifdef DONT_TRUST_BUMP_SECOND
				if (second > 58) {
					minute++;
					second = 0;
				}
#endif
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
			//if (act + pas == hw.freq || act + pas == 2 * hw.freq) {
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
			if (act > 0.4 * hw.freq) { // NPL
				bump_second = 0;
				second = 1;
				minute++;
				printf("%i:%i\n", minute, second);
			}
			if (act + pas > 0.8 * hw.freq) {
			//if (act + pas == hw.freq || act + pas == 2 * hw.freq) {
				act = pas = 0;
			}
		}
		if (change_interval) {
			change_interval = false;
#ifdef MODERN_API
#else
			itv.it_interval.tv_sec = 0;
			itv.it_interval.tv_usec = interval;
			(void)setitimer(ITIMER_REAL, &itv, NULL);
#endif
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
#ifdef MODERN_API
#else
		(void)pause();
#endif
	}

	cleanup();
	free(config);
	return EX_OK;
}
