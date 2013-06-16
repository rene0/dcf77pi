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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "input.h"

int
main(int argc, char **argv)
{
	unsigned long high, low, err;
	int fd;
	int old;
	int res;
	struct hardware hw;
#ifdef __FreeBSD__
	struct gpio_req req;
#endif

	res = read_hardware_parameters("hardware.txt", &hw);
	if (res)
		return res;

	fd = init_hardware(hw.pin);
	if (fd < 0)
		return fd; /* errno */

	high = low = err = 0;
	old = 0;
	for (;;) {
#ifdef __FreeBSD__
		req.gp_pin = hw.pin;
		if (ioctl(fd, GPIOGET, &req) < 0) {
			perror("ioctl(GPIOGET)");
#endif
			cleanup();
			return 1;
		}
#ifdef __FreeBSD__
		if ((req.gp_value != old && old == 0) ||
#endif
		    (high + low > hw.freq)) {
			printf("%lu %lu %lu\n", err, high, low);
			high = low = 0;
		}
#ifdef __FreeBSD__
		if (req.gp_value == GPIO_PIN_HIGH)
#endif
			high++;
#ifdef __FreeBSD__
		else if (req.gp_value == GPIO_PIN_LOW)
#endif
			low++;
		else
			err++; /* Houston? */
#ifdef __FreeBSD__
		old = req.gp_value;
#endif
		(void)usleep(1000000.0 / hw.freq); /* us */
	}
	if (close(fd))
		perror("close");
	return 0;
}
