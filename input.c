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
#include <sys/gpio.h>
#include "input.h"

int bitpos; /* second */
uint8_t buffer[60]; /* wrap after 60 positions */
int state; /* any errors, or high bit */
int islive; /* live input or pre-recorded data */
FILE *infile; /* input file (recorded data or hardware parameters) */
FILE *logfile; /* auto-appended in live mode */
int fd; /* gpio file */
int freq; /* number of samples per second */
int margin; /* absolute error margin */

int
set_mode(int live, char *filename)
{
	char gpioname[80];

	islive = live;
	bitpos = 0;
	state = 0;
	bzero(buffer, sizeof(buffer));

	if (live) {
		infile = fopen("hardware.txt", "r");
		if (infile == NULL) {
			printf("set_mode (hardware.txt): %s\n", strerror(errno));
			return errno;
		}
		if (fscanf(infile, "%s\n", gpioname) != 1) {
			printf("set_mode (gpio name): %s\n", strerror(errno));
			return errno;
		}
		if (fscanf(infile, "%i\n", &freq) != 1) {
			printf("set_mode (gpio freq): %s\n", strerror(errno));
			return errno;
		}
		if (fscanf(infile, "%i\n", &margin) != 1) {
			printf("set_mode (gpio margin): %s\n", strerror(errno));
			return errno;
		}
		if (fclose(infile) == EOF)
			printf("set_mode (hardware.txt): %s\n", strerror(errno));
		logfile = fopen("dcf77pi.log", "a");
		if (logfile == NULL) {
			printf("set_mode (logfile): %s\n", strerror(errno));
			return errno;
		}
		fprintf(logfile, "\n--new log--\n\n");
		fd = open(gpioname, O_RDONLY);
		if (fd < 0) {
			printf("set_mode (%s): %s\n", gpioname, strerror(errno));
			return errno;
		}
	} else {
		infile = fopen(filename, "r");
		if (infile == NULL) {
			printf("set_mode (infile): %s\n", strerror(errno));
			return errno;
		}
	}
	return 0;
}

void
cleanup(void)
{
	if (islive) {
		if (fclose(logfile) == EOF)
			printf("cleanup (logfile): %s\n", strerror(errno));
		if (close(fd) == -1)
			printf("cleanup (gpioc0): %s\n", strerror(errno));
	} else if (fclose(infile) == EOF)
			printf("cleanup (infile): %s\n", strerror(errno));
}

int
get_bit(void)
{
	char inch;
	int count, high, low;
	uint32_t oldval;
	struct gpio_req req;
	int seenlow = 0, seenhigh = 0;

	state = 0; /* clear previous flags */
	if (islive) {
/*
 * TODO nothing yet to actually receive the signal (DCF77 antenna over GPIO)
 *
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
		req.gp_pin = -1; /* TODO */
		oldval = 2; /* initially invalid */
		high = low = 0;
		for (count = 0; count < freq; count++) {
			if (ioctl(fd, GPIOGET, &req) < 0)
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
			if (oldval == 2)
				oldval = req.gp_value;
			if (oldval == req.gp_value) {
				if (req.gp_value == GPIO_PIN_HIGH)
					high++;
				else
					low++;
			} else if (oldval == GPIO_PIN_HIGH &&
			    req.gp_value == GPIO_PIN_LOW)
				seenlow++;
			else
				seenhigh++;
			
			oldval = req.gp_value;
			(void)usleep(1000000.0 / freq);
		}
		printf(" [%i %i %i %i %i]", freq, low, high, seenlow, seenhigh); /* freq should be low+high */
		if (high < margin)
			state |= GETBIT_EOM; /* ideally completely low */
		if (high >= (freq / 10.0) - margin &&
		    high <= (freq / 10.0) + margin)
			state |= 0; /* NOP, a zero bit */
		else if (high >= (freq / 5.0) - margin &&
		    high <= (freq / 5.0) + margin)
			state |= GETBIT_ONE;	
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
		if (feof(infile)) {
			state |= GETBIT_EOD;
			return state;
		}
		if (fscanf(infile, "%c", &inch) == 1) {
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
