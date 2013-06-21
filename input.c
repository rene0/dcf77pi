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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#ifdef __FreeBSD__
#include <sys/gpio.h>
#elif defined(__linux__)
#include <sys/types.h>
#else
#error Unsupported operating system, please send a patch to the author
#endif
#include "input.h"

int bitpos; /* second */
uint8_t buffer[60]; /* wrap after 60 positions */
int state; /* any errors, or high bit */
int islive; /* live input or pre-recorded data */
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
	if (fscanf(hwfile, "%i\n", &(_hw->margin)) != 1) {
		perror("gpio margin");
		return errno;
	}
	if (fscanf(hwfile, "%i\n", &(_hw->pin)) != 1) {
		perror("gpio pin");
		return errno;
	}
	if (fscanf(hwfile, "%i\n", &(_hw->min_len)) != 1) {
		perror("gpio min_len");
		return errno;
	}
	if (fscanf(hwfile, "%i\n", &(_hw->active_high)) != 1) {
		perror("gpio active_high");
		return errno;
	}
	if (fclose(hwfile) == EOF) {
		perror("fclose (hwfile)");
		return errno;
	}
	printf("hardware: freq=%li margin=%i pin=%i min_len=%i active_high=%i\n", _hw->freq, _hw->margin, _hw->pin, _hw->min_len, _hw->active_high);
	return 0;
}

int
init_hardware(int pin_nr)
{
#ifdef __FreeBSD__
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
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/direction", pin_nr);
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
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%d/value", pin_nr);
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
set_mode(int live, char *filename)
{
	int res;

	islive = live;
	bitpos = 0;
	state = 0;
	bzero(buffer, sizeof(buffer));

	signal(SIGINT, signal_callback_handler);

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

int
get_pulse(void)
{
	int count;
	char tmpch;
#ifdef __FreeBSD__
	struct gpio_req req;

	req.gp_pin = hw.pin;
	count = ioctl(fd, GPIOGET, &req);
	tmpch = (req.gp_value == GPIO_PIN_HIGH) ? 1 : 0;
	if (count < 0)
#elif defined(__linux__)
	count = read(fd, &tmpch, sizeof(tmpch));
	tmpch -= '0';
	lseek(fd, 0, SEEK_SET); /* rewind to prevent EBUSY/no read */
	if (count != sizeof(tmpch))
#endif
		return GETBIT_IO; /* hardware failure? */

	if (!hw.active_high)
		tmpch = 1 - tmpch;
	return tmpch;
}

int
get_bit(void)
{
	char inch;
	int count, high, low;
	int oldval;

	state = 0; /* clear previous flags */
	if (islive) {
/*
 * One period is either 1000 ms or 2000 ms long (normal or padding for last)
 * Active part is either 100 ms ('0') or 200 ms ('1') long, with an error
 * margin (e.g. 2%), so:
 *         A <   80 : too short    -> 920 <  ~A		GETBIT_READ
 *   80 <= A <= 120 : '0'          -> 880 <= ~A <= 920  -
 *  120 <  A <  180 : undetermined -> 820 <  ~A <  880  GETBIT_READ
 *  180 <= A <= 220 : '1'          -> 780 <= ~A <= 820  GETBIT_ONE
 *  220 <  A        : too long     ->        ~A <  780  GETBIT_READ
 *
 *  ~A > 1000 : value = GETBIT_EOM
 *  It is also possible that the signal is random active/passive, which means
 *  pulses shorter than 1000 ms or longer than 2000 ms e.g. due to thunderstorm,
 *  resulting in GETBIT_READ
 *
 *  maybe use bins as described at http://blog.blinkenlight.net/experiments/dcf77/phase-detection/
 */
		high = low = 0;
		oldval = 2; /* initial bogus value to prevent immediate break */

		for (;;) {
			inch = get_pulse();
			if (inch == GETBIT_IO) {
				state |= GETBIT_IO;
				break;
			}
			if (inch == 1)
				high++;
			else if (inch == 0)
				low++;
			if (inch != oldval && oldval == 2)
				oldval = inch; /* initialize */
			if ((inch != oldval && oldval == 0) || (high + low > hw.freq)) {
				/* TODO ragged signal (seems rather OK) , false GETBIT_XMIT */
				count = 100 * high / (high + low);
				if (islive == 2)
					printf("[%i %i %i]", high, low, count);
				break;
			}
			oldval = inch;
			(void)usleep(1000000.0 / hw.freq);
		}

		if (state & GETBIT_IO) {
			inch = '*';
			goto report;
		}

		if (high < 2) {
			state |= GETBIT_EOM;
			inch = '\n';
		}
		if (low < 2) {
			state |= GETBIT_XMIT;
			inch = 'x';
		}
		if (high > 1 && low > 1) {
			if (count >= (10 - hw.margin) && count <= (10 + hw.margin)) {
				/* zero bit, ~100 ms active signal */
				inch = '0';
				buffer[bitpos] = 0;
			} else if (count >= (20 - hw.margin) && count <= (20 + hw.margin)) {
				/* one bit, ~200 ms active signal */
				state |= GETBIT_ONE;
				inch = '1';
				buffer[bitpos] = 1;
			} else {
				state |= GETBIT_READ; /* bad radio signal */
				inch = '_';
			}
		}
report:
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
			case 'x' :
				state |= GETBIT_XMIT;
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
		printf(" ");
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
