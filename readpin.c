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
	int i, p, act;
	struct hardware hw;

	if (set_mode(0, NULL, NULL)) {
		cleanup();
		return 0;
	}

	(void)read_hardware_parameters("hardware.txt", &hw);
	/* get our own copy, error handling in set_mode() */

	printf("%i\n", (int)(hw.freq * hw.min_len / 100));
	for (i = act = 0;; i++) {
		p = get_pulse();
		if (p & GETBIT_IO) {
			printf("IO error!\n");
			break;
		}
		act += p;
		printf("%d", p);
		if (i == (int)(hw.freq * hw.min_len / 100)) {
			printf(" %i\n", act);
			i = act = 0;
		}
		(void)usleep(1000000.0 / hw.freq);
	}
	cleanup();
	return 0;
}
