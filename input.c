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

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>

#include <ncurses.h>

#ifdef __FreeBSD__
#  if __FreeBSD_version >= 900022
#    include <sys/gpio.h>
#  else
#    define NOLIVE 1
#  endif
#elif defined(__NetBSD_Version__)
#  define NOLIVE 1
#elif defined(__linux__)
#  include <sys/types.h>
#else
#  error Unsupported operating system, please send a patch to the author
#endif

#include "input.h"
#include "config.h"

uint8_t bitpos; /* second */
uint8_t buffer[60]; /* wrap after 60 positions */
uint16_t state; /* any errors, or high bit */
int islive; /* live input or pre-recorded data */
FILE *datafile = NULL; /* input file (recorded data) */
FILE *logfile = NULL; /* auto-appended in live mode */
int fd = 0; /* gpio file */
struct hardware hw;

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
set_mode(char *infilename, char *logfilename)
{
	int res;

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
	hw.maxzero = strtol(get_config_value("maxzero"), NULL, 10);
	hw.maxone = strtol(get_config_value("maxone"), NULL, 10);

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
		perror("close (/sys/class/gpio/*)");
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
	/*
	 * The bits are decoded from the signal using an exponential low-pass
	 * filter in conjunction with a Schmitt trigger. The idea and the
	 * initial implementation for this come from Udo Klein, with permission.
	 * http://blog.blinkenlight.net/experiments/dcf77/binary-clock/#comment-5916
	 */

	int inch, valid = 0, freq_reset = 0;
	char outch;
	int t, tlow, count, newminute;
	uint8_t p, stv;
	struct timespec slp;
	float a, y, w;
	static int init = 1;
	static float realfreq;

	/*
	 * Clear previous flags, except GETBIT_TOOLONG to be able
	 * to determine if this flag can be cleared again.
	 */
	state = (state & GETBIT_TOOLONG) ? GETBIT_TOOLONG : 0;
	if (islive) {
		/*
		 * One period is either 1000 ms or 2000 ms long (normal or
		 * padding for last). The active part is either 100 ms ('0')
		 * or 200 ms ('1') long, the maximum allowed values as
		 * percentage of the second length are specified with maxzero
		 * and maxone respectively.
		 *
		 *  ~A > 1.5 * realfreq: value |= GETBIT_EOM
		 *  ~A > 2.5 * realfreq: timeout
		 */

		/*
		 * Set up filter, reach 50% after realfreq/20 samples
		 * (i.e. 50 ms)
		 */
		if (init == 1)
			realfreq = hw.freq;
		a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
		y = -1;
		tlow = 0;
		stv = 2;
		w = 0.05;

		for (t = 0; ; t++) {
			p = get_pulse();
			if (p == GETBIT_IO) {
				state |= GETBIT_IO;
				outch = '*';
				goto report;
			}

			y = y < 0 ? (float)p : y + a * (p - y);
			if (stv == 2)
				stv = p;

			/*
			 * Prevent algorithm collapse during thunderstorms
			 * or scheduler abuse
			 */
			if (realfreq < hw.freq / 2) {
				if (islive == 0)
					printf("<");
				if (logfile)
					fprintf(logfile, "<");
				realfreq = hw.freq;
				freq_reset = 1;
			}
			if (realfreq > hw.freq * 3/2) {
				if (islive == 0)
					printf(">");
				if (logfile)
					fprintf(logfile, ">");
				realfreq = hw.freq;
				freq_reset = 1;
			}

			if (t > realfreq * 5/2) {
				realfreq = realfreq + w * ((t / 2.5) - realfreq);
				a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
				if (tlow * 100 / t < 1) {
					state |= GETBIT_RECV;
					outch = 'r';
				} else if (tlow * 100 / t >= 99) {
					state |= GETBIT_XMIT;
					outch = 'x';
				} else {
					state |= GETBIT_RND;
					outch = '#';
				}
				goto report; /* timeout */
			}

			/*
			 * Schmitt trigger, maximize value to introduce
			 * hysteresis and to avoid infinite memory.
			 */
			if (y < 0.5 && stv == 1) {
				y = 0.0;
				stv = 0;
				tlow = t; /* end of high part of second */
			}
			if (y > 0.5 && stv == 0) {
				y = 1.0;
				stv = 1;

				count = tlow * 100 / t;
				newminute = t > realfreq * 3/2;
				if (init == 1)
					init = 0;
				else {
					if (newminute) {
						realfreq = realfreq + w *
						    ((t/2) - realfreq);
					} else
						realfreq = realfreq + w *
						    (t - realfreq);
					a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
				}
				mvwprintw(input_win, 3, 0, "%3u  %4u (%2u%%)"
				   "  %11.6f   %8.6f", tlow, t, count,
				   realfreq, a);
				if (freq_reset)
					mvwchgat(input_win, 3, 17, 11, A_BOLD,
					   3, NULL);
				if (newminute) {
					count *= 2;
					state |= GETBIT_EOM;
				}
				break; /* start of new second */
			}
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
		if (logfile) {
			fprintf(logfile, "%c", outch);
			if (state & GETBIT_EOM)
				fprintf(logfile, "\n");
		}
		wrefresh(input_win);
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

int
is_space_bit(int bit)
{
	return (bit == 1 || bit == 15 || bit == 16 || bit == 19 ||
	    bit == 20 || bit == 21 || bit == 28 || bit == 29 ||
	    bit == 35 || bit == 36 || bit == 42 || bit == 45 ||
	    bit == 50 || bit == 58 || bit == 59 || bit == 60);
}

void
display_bit(void)
{
	if (is_space_bit(bitpos))
		printf(" ");
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
}

void
display_bit_gui(void)
{
	int xpos, i;

	mvwprintw(input_win, 3, 41, "%2u", bitpos);

	wattron(input_win, COLOR_PAIR(2));
	if (state & GETBIT_EOM)
		mvwprintw(input_win, 3, 44, "minute   ");
	else if (state < GETBIT_READ)
		mvwprintw(input_win, 3, 44, "OK       ");
	else
		mvwprintw(input_win, 3, 44, "         ");
	wattroff(input_win, COLOR_PAIR(2));

	wattron(input_win, COLOR_PAIR(1));
	if (state & GETBIT_RECV)
		mvwprintw(input_win, 3, 54, "receive ");
	else if (state & GETBIT_XMIT)
		mvwprintw(input_win, 3, 54, "transmit");
	else if (state & GETBIT_RND)
		mvwprintw(input_win, 3, 54, "random  ");
	else if (state & GETBIT_IO)
		mvwprintw(input_win, 3, 54, "IO      ");
	else
		mvwprintw(input_win, 3, 54, "        ");
	wattroff(input_win, COLOR_PAIR(1));

	for (xpos = bitpos + 4, i = 0; i <= bitpos; i++)
		if (is_space_bit(i))
			xpos++;

	mvwprintw(input_win, 0, xpos, "%u", buffer[bitpos]);
	if (state & GETBIT_READ)
		mvwchgat(input_win, 0, xpos, 1, A_BOLD, 3, NULL);
	wrefresh(input_win);
}

uint16_t
next_bit(int fromfile)
{
	if (state & GETBIT_EOM) {
		bitpos = 0;
		if (fromfile == 0) {
			mvwdelch(input_win, 0, 4);
			wclrtoeol(input_win);
			wrefresh(input_win);
		}
	} else
		bitpos++;
	if (bitpos == sizeof(buffer)) {
		state |= GETBIT_TOOLONG;
		bitpos = 0;
		if (fromfile)
			printf(" L");
		else {
			wattron(input_win, COLOR_PAIR(1));
			mvwprintw(input_win, 3, 44, "no minute");
			wattroff(input_win, COLOR_PAIR(1));
			wrefresh(input_win);
		}
		return state;
	}
	state &= ~GETBIT_TOOLONG; /* fits again */
	if (fromfile == 0)
		wrefresh(input_win);
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
