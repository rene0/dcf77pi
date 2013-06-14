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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "input.h"

int bitpos; /* second */
uint8_t buffer[60]; /* wrap after 60 positions */
int state; /* any errors, or high bit */
int islive; /* live input or pre-recorded data */
FILE *datafile = NULL; /* input file (recorded data) */
FILE *logfile = NULL; /* auto-appended in live mode */
int fd = 0; /* gpio file */

struct hardware hw;

int
read_hardware_parameters(char *filename, struct hardware *_hw)
{
	FILE *hwfile;

	hwfile = fopen(filename, "r");
	if (hwfile == NULL) {
		perror("fopen (hwfile)");
		return errno;
	}
	if (fscanf(hwfile, "%li\n", &(_hw->freq)) != 1) {
		perror("gpio freq");
		return errno;
	}
	if (fscanf(hwfile, "%li\n", &(_hw->margin)) != 1) {
		perror("gpio margin");
		return errno;
	}
	if (fscanf(hwfile, "%i\n", &(_hw->pin)) != 1) {
		perror("gpio pin");
		return errno;
	}
	if (fclose(hwfile) == EOF) {
		perror("fclose (hwfile)");
		return errno;
	}
	return 0;
}

int
init_hardware(int pin_nr)
{
	struct gpio_pin pin;

	fd = open("/dev/gpioc0", O_RDONLY);
	if (fd < 0) {
		perror("open (gpioc0)");
		return errno;
	}

	pin.gp_pin = pin_nr;
	if (ioctl(fd, GPIOGETCONFIG, &pin) < 0) {
		perror("ioctl(GPIOGETCONFIG)");
		return errno;
	}

	return fd;
}

int
set_mode(int live, char *filename)
{
	int res;

	islive = live;
	bitpos = 0;
	state = 0;
	bzero(buffer, sizeof(buffer));

	if (live) {
		res = read_hardware_parameters("hardware.txt", &hw);
		if (res) {
			cleanup();
			return res;
		}
		res = init_hardware(hw.pin);
		if (res < 0) {
			cleanup();
			return res;
		}
		logfile = fopen("dcf77pi.log", "a");
		if (logfile == NULL) {
			perror("fopen (logfile)");
			cleanup();
			return errno;
		}
		fprintf(logfile, "\n--new log--\n\n");
	} else {
		datafile = fopen(filename, "r");
		if (datafile == NULL) {
			perror("fopen (datafile)");
			return errno;
		}
	}
	return 0;
}

void
cleanup(void)
{

#ifdef __FreeBSD__
	if (fd > 0 && close(fd) == -1)
		perror("close (gpioc0)");
#endif
	if (logfile != NULL && fclose(logfile) == EOF)
		perror("fclose (logfile)");
	if (datafile != NULL && fclose(datafile) == EOF)
		perror("fclose (datafile)");
}

int
get_bit(void)
{
	char inch;
	int count, high, low;
	uint32_t oldval;
	int seenlow = 0, seenhigh = 0;
#ifdef __FreeBSD__
	struct gpio_req req;
#endif

	state = 0; /* clear previous flags */
	if (islive) {
/*
 * One pulse is either 1000 ms or 2000 ms long (normal or padding for last)
 * Active part is either 100 ms ('0') or 200 ms ('1') long, with an error
 * certain margin (e.g. 30 ms), so:
 *         A <   70 : too short    -> 930 <  ~A		GETBIT_READ
 *   70 <= A <= 130 : '0'          -> 870 <= ~A <= 930  -
 *  130 <  A <  170 : undetermined -> 830 <  ~A <  870  GETBIT_READ
 *  170 <= A <= 230 : '1'          -> 770 <= ~A <= 830  GETBIT_ONE
 *  230 <  A        : too long     ->        ~A <  770  GETBIT_READ
 *
 *  ~A > 1000 : value = 2 GETBIT_EOM
 *  It is also possible that the signal is random active/passive, which means
 *  pulses shorter than 1000 ms or longer than 2000 ms e.g. due to thunderstorm
 *  GETBIT_READ
 *
 *  maybe use bins as described at http://blog.blinkenlight.net/experiments/dcf77/phase-detection/
 */
#ifdef __FreeBSD__
		req.gp_pin = hw.pin;
#endif
		oldval = 0;
		high = low = 0;
//XXX wrong, we have to detect the edges
		for (count = 0; count < hw.freq; count++) {
#ifdef __FreeBSD__
			if (ioctl(fd, GPIOGET, &req) < 0)
#endif
				state |= GETBIT_IO; /* ioctl error */
			/*
			* result in req.gp_value
			* TODO
			* remaining low
			* detect /
			* count high (A above)
			* detect \
			* count low (~A above)
			*
			* / and \ should only happen once each loop,
			* otherwise state |= GETBIT_READ;
			*
			* if no / happens, then state |= GETBIT_EOM;
			*/
#ifdef __FreeBSD__
			if (oldval == req.gp_value) {
				if (req.gp_value == GPIO_PIN_HIGH)
#else
			if (oldval == oldval) { /* TODO */
				if (0) /* TODO */
#endif
					high++;
				else
					low++;
#if __FreeBSD__
			} else if (oldval == GPIO_PIN_HIGH &&
			    req.gp_value == GPIO_PIN_LOW)
#else
			} else if (0) /* TODO */
#endif
				seenlow++;
			else
				seenhigh++;
			
#ifdef __FreeBSD__
			oldval = req.gp_value;
#else
			oldval = oldval; /* TODO */
#endif
			(void)usleep(1000000.0 / hw.freq);
		}
		printf(" [%lu %i %i %i %i]", hw.freq, low, high, seenlow, seenhigh); /* hw.freq should be low+high */
		if (high < hw.margin)
			state |= GETBIT_EOM; /* ideally completely low */
		if (high >= (hw.freq / 10.0) - hw.margin &&
		    high <= (hw.freq / 10.0) + hw.margin)
			state |= 0; /* NOP, a zero bit, ~100 ms active signal */
		else if (high >= (hw.freq / 5.0) - hw.margin &&
		    high <= (hw.freq / 5.0) + hw.margin)
			state |= GETBIT_ONE; /* one bit, ~200 ms active signal */
		else
			state |= GETBIT_READ; /* something went wrong */
		
		if (state & GETBIT_EOM)
			inch = '\n';
		else if (state & GETBIT_IO)
			inch = '*';
		else if (state & GETBIT_READ)
			inch = '_';
		else if (state & GETBIT_ONE)
			inch = '1';
		else
			inch = '0';
		fprintf(logfile, "%c", inch);
	} else {
		if (feof(datafile)) {
			state |= GETBIT_EOD;
			return state;
		}
		if (fscanf(datafile, "%c", &inch) == 1) {
			switch (inch) {
			case '0':
			case '1':
				buffer[bitpos] = (uint8_t)(inch - '0');
				if (inch == '1')
					state |= GETBIT_ONE;
				break;
			case '\n' :
				state |= GETBIT_EOM;
				break;
			case '*' :
				state |= GETBIT_IO;
				break;
			default:
				state |= GETBIT_READ;
				break;
			}
		} else
			state |= GETBIT_READ;
	}
	return state;
}

void
display_bit(void)
{
	printf("%d", buffer[bitpos]);
	if (bitpos == 0 || bitpos == 14 || bitpos == 15 || bitpos == 18 ||
	    bitpos == 19 || bitpos == 20 || bitpos == 27 || bitpos == 28 ||
	    bitpos == 34 || bitpos == 35 || bitpos == 41 || bitpos == 44 ||
	    bitpos == 49 || bitpos == 57 || bitpos == 58)
		printf("%c", ' ');
}

int
next_bit(void)
{
	if (bitpos == sizeof(buffer) && ((state & GETBIT_EOM) == 0)) {
		bitpos = 0;
		state |= GETBIT_TOOLONG | GETBIT_EOM;
	}
	if (state & GETBIT_EOM)
		bitpos = 0;
	else if ((state & GETBIT_READ) == 0)
		bitpos++;
	return state;
}

int
get_bitpos(void)
{
	return bitpos;
}

uint8_t*
get_buffer(void)
{
	return buffer;
}
