// Copyright 2013-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"
#include "mainloop_live.h"

#include "json_util.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

static struct json_object *config;
static struct hardware hw;
static volatile sig_atomic_t running = 1;
static int min = -1;
static bool verbose = true;

/** filler method */
void
nope()
{
}

/** another filler method */
struct ML_result
nope_mlr(struct ML_result _1, int _2)
{
	struct ML_result nothing = {};
	return nothing;
}

void
display_long_minute(void)
{
	printf("Too long minute!\n");
	min++;
}

void
display_minute(int _1)
{
	min++;
}

void
display_bit(struct GB_result gbr, int bitpos)
{
	printf("%i:%i %i %i %li %f %f %i ", min, bitpos, gbr.bitval,
	    bitinfo.act, bitinfo.interval, bitinfo.bit0, bitinfo.bit20,
	    bitinfo.cursor);
	if (verbose) {
		int i;

		/* display all valid pulses */
		for (i = 0; i < bitinfo.cursor / 8; i++) {
			for (int j = 0; j < 8; j++) {
				printf("%c", (bitinfo.signal[i] & (1 << j)) >
				    0 ? '+' : '-');
			}
		}
		/*
		 * display pulses in the last partially filled item
		 * cursor is 0-based, hence the <= comparison
		 */
		i = bitinfo.cursor / 8;
		for (int j = 0; j <= (bitinfo.cursor & 7); j++) {
			printf("%c", (bitinfo.signal[i] & (1 << j)) >
			    0 ? '+' : '-');
		}
	}
	printf("\n");
}

static void
client_cleanup()
{
	/* Caller is supposed to exit the program after this. */
	cleanup();
	free(config);
}

static void
sigint_handler(int _1)
{
	running = 0;
}

int
main(int argc, char *argv[])
{
	struct sigaction sigact;
	int ch, res;
	bool raw = false;

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

#if 0 //SIGALRM from mainloop_live eats this SIGINT
	sigact.sa_handler = sigint_handler;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, NULL);
#endif
	if (raw) {
		while (running == 1) {
			struct timespec slp;
			slp.tv_sec = 0;
			slp.tv_nsec = 1e9 / hw.freq;
			printf("%i", get_pulse());
			fflush(stdout);
			while (nanosleep(&slp, &slp) > 0)
				; /* empty loop */
			continue;
		}
	} else {
		mainloop_live(NULL, display_bit, display_long_minute,
		    display_minute, nope, nope, nope, nope, nope, nope,
		    nope_mlr, nope_mlr, nope_mlr);
	}

	client_cleanup();
	printf("done\n");
	return 0;
}
