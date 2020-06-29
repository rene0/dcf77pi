// Copyright 2013-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"

#include "json_util.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

static struct json_object *config;
static volatile sig_atomic_t running = 1;

static void
client_cleanup()
{
	/* Caller is supposed to exit the program after this. */
	cleanup();
	free(config);
}

static void
sigint_handler(/*@unused@*/ int sig)
{
	running = 0;
}

int
main(int argc, char *argv[])
{
	struct sigaction sigact;
	struct hardware hw;
	int ch, min, res;
	bool raw = false, verbose = true;

	while ((ch = getopt(argc, argv, "qr")) != -1) {
		switch (ch) {
		case 'q':
			verbose = false;
			break;
		case 'r':
			raw = true;
			break;
		default:
			printf("usage: %s [-qr]\n", argv[0]);
			return EX_USAGE;
		}
	}

	config = json_object_from_file(ETCDIR "/config.json");
	if (config == NULL) {
		client_cleanup();
		return EX_NOINPUT;
	}
	res = set_mode_live(config);
	if (res != 0) {
		client_cleanup();
		return res;
	}
	hw = get_hardware_parameters();

	sigact.sa_handler = sigint_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);

	min = -1;

	while (running == 1) {
#if 0
		struct bitinfo bi;
		struct GB_result gbr;
#endif
		if (raw) {
			struct timespec slp;
			slp.tv_sec = 0;
			slp.tv_nsec = 1e9 / hw.freq;
			printf("%i", get_pulse());
			fflush(stdout);
			while (nanosleep(&slp, &slp) > 0)
				; /* empty loop */
			continue;
		}
/*
 * Below code is similar itimer-signal-demo.c which probably needs to be made
 * into a callback to mainloop_live(), or mainloop_live() needs a verbose
 * mode.
 */
#if 0
		gbr = get_bit_live();
		bi = get_bitinfo();
		if (verbose) {
			if (bi.change_interval) {
				printf("!");
			}
			/* display first bi->t pulses */
			for (unsigned long long i = 0; i < bi.t / 8; i++) {
				for (unsigned j = 0; j < 8; j++) {
					printf("%c", (bi.signal[i] & (1 << j)) >
					    0 ? '+' : '-');
				}
			}
			/*
			 * display pulses in the last partially filled item
			 * bi.t is 0-based, hence the <= comparison
			 */
			for (unsigned j = 0; j <= (bi.t & 7); j++) {
				printf("%c", (bi.signal[bi.t / 8] & (1 << j)) >
				    0 ? '+' : '-');
			}
			printf("\n");
		}
		if (gbr.marker == emark_toolong || gbr.marker == emark_late) {
			printf("Too long minute!\n");
			min++;
		}
		printf("%i %i %i %u %f %f %i:%i\n", gbr.bitval, bi.tlow,
		    bi.tlast0, bi.t, bi.bit0, bi.bit20, min,
		    get_bitpos());
		if (gbr.marker == emark_minute) {
			min++;
		}
		gbr = next_bit(gbr);
#endif
	}

	client_cleanup();
	printf("done\n");
	return 0;
}
