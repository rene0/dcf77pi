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

#include "config.h"
#include "input.h"

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

void
do_cleanup(int sig)
{
	cleanup();
	printf("done\n");
	exit(0);
}

int
main(int argc, char **argv)
{
	int min, res, verbose = 1;
	uint16_t bit;
	struct bitinfo *bi;
	struct sigaction sigact;

	if ((argc == 2) && !strncmp(argv[1], "-q", strlen(argv[1])))
		verbose = 0;
	else if (argc != 1) {
		printf("usage: %s [-q]\n", argv[0]);
		return EX_USAGE;
	}

	res = read_config_file(ETCDIR"/config.txt");
	if (res != 0) {
		cleanup();
		return res;
	}
	res = set_mode_live();
	if (res != 0) {
		cleanup();
		return res;
	}

	sigact.sa_handler = do_cleanup;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGINT, &sigact, (struct sigaction *)NULL);

	min = -1;

	for (;;) {
		bit = get_bit_live();
		bi = get_bitinfo();
		if (verbose) {
			int i, j;

			if (bi->freq_reset)
				printf("!");
			/* display first bi->t pulses */
			for (i = 0; i < bi->t / 8; i++)
				for (j = 0; j < 8; j++)
					printf("%c", bi->signal[i] & (1 << j) ?
					    '+' : '-');
			/*
			 * display pulses in the last partially filled item
			 * bi->t is 0-based, hence the <= comparison
			 */
			for (j = 0; j <= (bi->t & 7); j++)
				printf("%c", bi->signal[bi->t / 8] & (1 << j) ?
				    '+' : '-');
			printf("\n");
		}
		if (bit & GETBIT_TOOLONG)
			min++;
		printf("%x (%"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64" %"PRIi64
		    " %"PRIi64") %i:%i\n", bit, bi->tlow, bi->tlast0, bi->t,
		    bi->bit0, bi->bit20, bi->realfreq, min, get_bitpos());
		if (bit & GETBIT_EOM)
			min++;
		bit = next_bit();
	}
	// NOTREACHED
}
