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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/param.h>
#if __FreeBSD_version >= 900022
#include <sys/gpio.h>
#else
#define NOLIVE 1
#endif
#elif defined(__linux__)
#include <sys/types.h>
#else
#error Unsupported operating system, please send a patch to the author
#endif
#include "input.h"
#include "config.h"

uint8_t bitpos; /* second */
uint8_t buffer[60]; /* wrap after 60 positions */
uint16_t state; /* any errors, or high bit */
int islive; /* live input or pre-recorded data */
int isverbose; /* verbose live information */
FILE *datafile = NULL; /* input file (recorded data) */
FILE *logfile = NULL; /* auto-appended in live mode */
int fd = 0; /* gpio file */
struct hardware hw;

void
signal_callback_handler(int signum)
{
	printf("Caught signal %d\n", signum);
	cleanup();
	exit(signum);
}

int
init_hardware(int pin_nr)
{
#ifdef __FreeBSD__
#ifndef NOLIVE
	struct gpio_pin pin;

	fd = open("/dev/gpioc0", O_RDONLY);
	if (fd < 0) {
		perror("open (/dev/gpioc0)");
		return -errno;
	}

	pin.gp_pin = pin_nr;
	if (ioctl(fd, GPIOGETCONFIG, &pin) < 0) {
		perror("ioctl(GPIOGETCONFIG)");
		return -errno;
	}
#endif
#elif defined(__linux__)
	char buf[64];
	int res;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd < 0) {
		perror("open (/sys/class/gpio/export)");
		return -errno;
	}
	res = snprintf(buf, sizeof(buf), "%d", pin_nr);
	if (res < 0 || res > sizeof(buf)-1) {
		printf("pin_nr too high? (%i)\n", res);
		return -1;
	}
	if (write(fd, buf, res) < 0) {
		perror("write(export)");
		if (errno != EBUSY)
			return -errno; /* EBUSY -> pin already exported ? */
	}
	if (close(fd) == -1) {
		perror("close(export)");
		return -errno;
	}
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction",
	    pin_nr);
	if (res < 0 || res > sizeof(buf)-1) {
		printf("pin_nr too high? (%i)\n", res);
		return -1;
	}
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("open (direction)");
		return -errno;
	}
	if (write(fd, "in", 3) < 0) {
		perror("write(in)");
		return -errno;
	}
	if (close(fd) == -1) {
		perror("close(direction)");
		return -errno;
	}
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value",
	    pin_nr);
	if (res < 0 || res > sizeof(buf)-1) {
		printf("pin_nr too high? (%i)\n", res);
		return -1;
	}
	fd = open(buf, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open (value)");
		return -errno;
	}
#endif
	return fd;
}

int
set_mode(int verbose, char *infilename, char *logfilename)
{
	int res;

	isverbose = verbose;
	islive = (infilename == NULL);
#ifdef NOLIVE
	if (islive) {
		printf("No GPIO interface available, disabling live decoding\n");
		cleanup();
		return 1;
	}
#endif
	bitpos = 0;
	state = 0;
	bzero(buffer, sizeof(buffer));

	/* fill hardware structure */
	hw.pin = strtol(get_config_value("pin"), NULL, 10);
	hw.active_high = strtol(get_config_value("activehigh"), NULL, 10);
	hw.freq = strtol(get_config_value("freq"), NULL, 10);
	hw.realfreq = strtol(get_config_value("realfreq"), NULL, 10);
	hw.maxzero = strtol(get_config_value("maxzero"), NULL, 10);
	hw.maxone = strtol(get_config_value("maxone"), NULL, 10);

	(void)signal(SIGINT, signal_callback_handler);

	if (islive) {
		res = init_hardware(hw.pin);
		if (res < 0) {
			cleanup();
			return res;
		}
		if (logfilename != NULL) {
			logfile = fopen(logfilename, "a");
			if (logfile == NULL) {
				perror("fopen (logfile)");
				cleanup();
				return errno;
			}
			fprintf(logfile, "\n--new log--\n\n");
		}
	} else {
		datafile = fopen(infilename, "r");
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
	if (fd > 0 && close(fd) == -1)
#ifdef __FreeBSD__
		perror("close (/dev/gpioc0)");
#elif defined(__linux__)
		perror("close (/sys/class/gpio/*");
#endif
	fd = 0;
	if (logfile != NULL && fclose(logfile) == EOF)
		perror("fclose (logfile)");
	logfile = NULL;
	if (datafile != NULL && fclose(datafile) == EOF)
		perror("fclose (datafile)");
	datafile = NULL;
}

uint8_t
get_pulse(void)
{
	uint8_t tmpch = 0;
	int count = 0;
#ifdef __FreeBSD__
#ifndef NOLIVE
	struct gpio_req req;

	req.gp_pin = hw.pin;
	count = ioctl(fd, GPIOGET, &req);
	tmpch = (req.gp_value == GPIO_PIN_HIGH) ? 1 : 0;
	if (count < 0)
#endif
#elif defined(__linux__)
	count = read(fd, &tmpch, sizeof(tmpch));
	tmpch -= '0';
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
		return GETBIT_IO; /* rewind to prevent EBUSY/no read */
	if (count != sizeof(tmpch))
#endif
		return GETBIT_IO; /* hardware failure? */

	if (!hw.active_high)
		tmpch = 1 - tmpch;
	return tmpch;
}

uint16_t
get_bit(void)
{
	int inch, valid = 0;
	char outch;
	int count, high, low;
	uint8_t p, p0;
	int minlimit, maxlimit;
	static int init = 1;
	struct timespec slp;

	/* clear previous flags, except GETBIT_TOOLONG to be able
	 * to determine if this flag can be cleared again.
	 */
	state = (state & GETBIT_TOOLONG) ? GETBIT_TOOLONG : 0;
	if (islive) {
/*
 * One period is either 1000 ms or 2000 ms long (normal or padding for last)
 * Active part is either 100 ms ('0') or 200 ms ('1') long, with an error
 * margin (e.g. 2%), so:
 *    0 <  A <   20 : undetermined -> GETBIT_READ
 *   20 <  A <= 120 : '0'          -> -
 *  120 <  A <  180 : undetermined -> GETBIT_READ
 *  180 <= A <  240 : '1'          -> GETBIT_ONE
 *  240 <= A        : undetermined -> GETBIT_READ
 *
 *  ~A > 1000 : value |= GETBIT_EOM
 * Short pulses (< min_len % of the sampling frequency) are concatenated.
 *
 *  maybe use bins as described at http://blog.blinkenlight.net/experiments/dcf77/phase-detection/
 */
		high = low = 0;
		p0 = 255;
		minlimit = hw.freq * hw.min_len / 100;
		maxlimit = hw.freq * hw.max_len / 100;

		for (i = 0; ; i++) {
			p = get_pulse();
			if (p == GETBIT_IO) {
				state |= GETBIT_IO;
				outch = '*';
				goto report;
			}
			if (p == 0)
				low++;
			if (p == 1)
				high++;
			if (p0 != p) {
				if (p0 == 0) { /* skip initial situation */
					count = high * 100/i;
					if (i > minlimit * 2 &&
					    i < maxlimit * 2)
						count *= 2;
					if (isverbose)
						printf("[%u %d %u", i, count, bitpos);
					if (i > minlimit && (init == 1 ||
					    i < maxlimit)) {
						/* new second */
						init = 0;
						break;
					} else if (i > minlimit * 2 && (init == 1 ||
					    i < maxlimit * 2)) {
						/* new minute */
						state |= GETBIT_EOM;
						init = 0;
						break;
					} else if (init == 1) {
						/* end of partial second */
						init = 0;
						break;
					}
					if (i > maxlimit / 5) {
						high--;
						low++;
					}
				}
				if (p0 == 1) {
					if (i < maxlimit / 10) {
						high++;
						low--;
					}
				}
			}
			if (i > maxlimit * 2) {
				if (isverbose)
					printf("{%i %i}", high, i); /* timeout */
				if (high < 2) {
					state |= GETBIT_RECV;
					outch = 'r';
				} else if (low < 2) {
					state |= GETBIT_XMIT;
					outch = 'x';
				} else {
					state |= GETBIT_RND;
					outch = '#';
				}
				goto report;
			}
			p0 = p;
			slp.tv_sec = 0;
			slp.tv_nsec = 1e9 / hw.freq;
			while (nanosleep(&slp, &slp))
				;
		}

		if (count <= hw.maxzero) {
			/* zero bit, ~100 ms active signal */
			outch = '0';
			buffer[bitpos] = 0;
		} else if (count <= hw.maxone) {
			/* one bit, ~200 ms active signal */
			state |= GETBIT_ONE;
			outch = '1';
			buffer[bitpos] = 1;
		} else {
			/* bad radio signal, retain old value */
			state |= GETBIT_READ;
			outch = '_';
		}
report:
		if (isverbose)
			printf(" %u]", state);
		if (logfile) {
			fprintf(logfile, "%c", outch);
			if (state & GETBIT_EOM)
				fprintf(logfile, "\n");
		}
	} else {
		while (valid == 0) {
			inch = getc(datafile);
			switch (inch) {
			case EOF:
				state |= GETBIT_EOD;
				return state;
			case '0':
			case '1':
				buffer[bitpos] = (uint8_t)(inch - '0');
				if (inch == '1')
					state |= GETBIT_ONE;
				valid = 1;
				break;
			case '\r' :
			case '\n' :
				state |= GETBIT_EOM; /* otherwise empty bit */
				valid = 1;
				break;
			case 'x' :
				state |= GETBIT_XMIT;
				valid = 1;
				break;
			case 'r':
				state |= GETBIT_RECV;
				valid = 1;
				break;
			case '#' :
				state |= GETBIT_RND;
				valid = 1;
				break;
			case '*' :
				state |= GETBIT_IO;
				valid = 1;
				break;
			case '_' :
				/* retain old value in buffer[bitpos] */
				state |= GETBIT_READ;
				valid = 1;
				break;
			default:
				break;
			}
		}
		inch = getc(datafile);
		if (inch == '\n')
			state |= GETBIT_EOM;
		else {
			if (inch == '\r') {
				state |= GETBIT_EOM;
				inch = getc(datafile);
				if (inch != '\n' &&
				    ungetc(inch, datafile) == EOF)
					state |= GETBIT_EOD;
					/* push back, not an EOM marker */
			} else if (ungetc(inch, datafile) == EOF)
				state |= GETBIT_EOD;
				/* push back, not an EOM marker */
		}
	}
	return state;
}

void
display_bit(void)
{
	if (state & GETBIT_RECV)
		printf("r");
	else if (state & GETBIT_XMIT)
		printf("x");
	else if (state & GETBIT_RND)
		printf("#");
	else if (state & GETBIT_READ)
		printf("_");
	else
		printf("%u", buffer[bitpos]);
	if (bitpos == 0 || bitpos == 14 || bitpos == 15 || bitpos == 18 ||
	    bitpos == 19 || bitpos == 20 || bitpos == 27 || bitpos == 28 ||
	    bitpos == 34 || bitpos == 35 || bitpos == 41 || bitpos == 44 ||
	    bitpos == 49 || bitpos == 57 || bitpos == 58 || bitpos == 59)
		printf(" ");
}

uint16_t
next_bit(void)
{
	if (state & GETBIT_EOM)
		bitpos = 0;
	else
		bitpos++;
	if (bitpos == sizeof(buffer)) {
		state |= GETBIT_TOOLONG;
		bitpos = 0;
		return state;
	}
	state &= ~GETBIT_TOOLONG; /* fits again */
	return state;
}

uint8_t
get_bitpos(void)
{
	return bitpos;
}

uint8_t *
get_buffer(void)
{
	return buffer;
}

struct hardware *
get_hardware_parameters(void)
{
	return &hw;
}
