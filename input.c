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

#include "input.h"

#include "config.h"
#include "guifuncs.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <ncurses.h>

#include <sys/param.h>

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

uint8_t bitpos; /* second */
uint8_t buffer[60]; /* wrap after 60 positions */
uint16_t state; /* any errors, or high bit */
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

void
set_state_vars(void)
{
	bitpos = 0;
	state = 0;
	bzero(buffer, sizeof(buffer));
}

int
set_mode_file(char *infilename)
{
	set_state_vars();
	datafile = fopen(infilename, "r");
	if (datafile == NULL) {
		perror("fopen (datafile)");
		return errno;
	}
	return 0;
}

int
set_mode_live(void)
{
	int res;

	set_state_vars();
#ifdef NOLIVE
	printf("No GPIO interface available, disabling live decoding\n");
	cleanup();
	return -1;
#else
	/* fill hardware structure and initialize hardware */
	hw.pin = strtol(get_config_value("pin"), NULL, 10);
	res = init_hardware(hw.pin);
	if (res < 0) {
		cleanup();
		return res;
	}
	hw.active_high = strtol(get_config_value("activehigh"), NULL, 10);
	hw.freq = strtol(get_config_value("freq"), NULL, 10);
#endif
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
get_bit_live(void)
{
	/*
	 * The bits are decoded from the signal using an exponential low-pass
	 * filter in conjunction with a Schmitt trigger. The idea and the
	 * initial implementation for this come from Udo Klein, with permission.
	 * http://blog.blinkenlight.net/experiments/dcf77/binary-clock/#comment-5916
	 */

	int freq_reset = 0;
	char outch;
	int t, tlow, newminute;
	uint8_t p, stv;
	struct timespec slp;
	float a, y, frac = -0.01, maxone = -0.01;
	static int init = 1;
	static float realfreq, bit0, bit20;

	/*
	 * Clear previous flags, except GETBIT_TOOLONG to be able
	 * to determine if this flag can be cleared again.
	 */
	state = (state & GETBIT_TOOLONG) ? GETBIT_TOOLONG : 0;

	/*
	 * One period is either 1000 ms or 2000 ms long (normal or
	 * padding for last). The active part is either 100 ms ('0')
	 * or 200 ms ('1') long, the maximum allowed values as
	 * percentage of the second length are specified with maxone/2
	 * and maxone respectively.
	 *
	 *  ~A > 1.5 * realfreq: value |= GETBIT_EOM
	 *  ~A > 2.5 * realfreq: timeout
	 */

	if (init == 1) {
		realfreq = hw.freq;
		bit0 = 0.1 * realfreq;
		bit20 = 0.2 * realfreq;
	}
	/*
	 * Set up filter, reach 50% after realfreq/20 samples (i.e. 50 ms)
	 */
	a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
	y = -1;
	tlow = 0;
	stv = 2;

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
		if (realfreq < hw.freq / 2 || realfreq > hw.freq * 3/2) {
			if (logfile != NULL)
				fprintf(logfile, realfreq < hw.freq / 2 ?
				    "<" : ">");
			realfreq = hw.freq;
			freq_reset = 1;
		}

		if (t > realfreq * 5/2) {
			realfreq = realfreq + 0.05 * ((t / 2.5) - realfreq);
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

			newminute = t > realfreq * 3/2;
			if (init == 1)
				init = 2;
			else {
				if (newminute)
					realfreq = realfreq + 0.05 *
					    ((t/2) - realfreq);
				else
					realfreq = realfreq + 0.05 *
					    (t - realfreq);
				a = 1.0 - exp2(-1.0 / (realfreq / 20.0));
			}

			frac = (float)tlow / (float)t;
			if (newminute) {
				frac *= 2;
				state |= GETBIT_EOM;
			}
			break; /* start of new second */
		}
		slp.tv_sec = 0;
		slp.tv_nsec = 1e9 / hw.freq;
		while (nanosleep(&slp, &slp))
			;
	}

	maxone = (bit0 + bit20) / realfreq;
	if (frac < 0) {
		/* radio error, results already set */
	} else if (frac <= maxone / 2.0) {
		/* zero bit, ~100 ms active signal */
		outch = '0';
		buffer[bitpos] = 0;
	} else if (frac <= maxone) {
		/* one bit, ~200 ms active signal */
		state |= GETBIT_ONE;
		outch = '1';
		buffer[bitpos] = 1;
	} else {
		/* bad radio signal, retain old value */
		state |= GETBIT_READ;
		outch = '_';
	}
	if (init == 2)
		init = 0;
	else {
		if (bitpos == 0 && buffer[0] == 0)
			bit0 = bit0 + 0.5 * (tlow - bit0);
		if (bitpos == 20 && buffer[20] == 1)
			bit20 = bit20 + 0.5 * (tlow - bit20);
	}
report:
	if (logfile != NULL)
		fprintf(logfile, "%c%s", outch, state & GETBIT_EOM ? "\n" : "");
	mvwprintw(input_win, 3, 4, "%4u  %4u (%5.1f%%) %8.3f"
	    " %4u %4u %4.1f%%  %8.6f", tlow, t, frac * 100,
	    realfreq, (int)bit0, (int)bit20, maxone * 100, a);
	if (freq_reset)
		mvwchgat(input_win, 3, 24, 8, A_BOLD, 3, NULL);
	else
		mvwchgat(input_win, 3, 24, 8, A_NORMAL, 7, NULL);
	wrefresh(input_win);
	return state;
}

#define TRYCHAR \
	oldinch = inch; \
	inch = getc(datafile); \
	if (feof(datafile)) \
		return state; \
	if ((inch != '\r' && inch != '\n') || inch == oldinch) { \
		if (ungetc(inch, datafile) == EOF) /* EOF remains, IO error */\
			state |= GETBIT_EOD; \
	}

uint16_t
get_bit_file(void)
{
	int oldinch, inch, valid = 0;

	/*
	 * Clear previous flags, except GETBIT_TOOLONG to be able
	 * to determine if this flag can be cleared again.
	 */
	state = (state & GETBIT_TOOLONG) ? GETBIT_TOOLONG : 0;

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
		case '\r':
		case '\n':
			/* handle multiple consecutive EOM markers */
			state |= GETBIT_EOM; /* otherwise empty bit */
			valid = 1;
			break;
		case 'x':
			state |= GETBIT_XMIT;
			valid = 1;
			break;
		case 'r':
			state |= GETBIT_RECV;
			valid = 1;
			break;
		case '#':
			state |= GETBIT_RND;
			valid = 1;
			break;
		case '*':
			state |= GETBIT_IO;
			valid = 1;
			break;
		case '_':
			/* retain old value in buffer[bitpos] */
			state |= GETBIT_READ;
			valid = 1;
			break;
		default:
			break;
		}
	}
	/* Only allow \r , \n , \r\n , and \n\r as single EOM markers */
	TRYCHAR else {
		state |= GETBIT_EOM;
		/* Check for B\r\n or B\n\r */
		TRYCHAR
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
display_bit_file(void)
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

	mvwprintw(input_win, 3, 1, "%2u", bitpos);

	wattron(input_win, COLOR_PAIR(2));
	if (state & GETBIT_EOM)
		mvwprintw(input_win, 3, 59, "minute   ");
	else if (state == 0 || state == GETBIT_ONE)
		mvwprintw(input_win, 3, 59, "OK       ");
	else
		mvwprintw(input_win, 3, 59, "         ");
	wattroff(input_win, COLOR_PAIR(2));

	wattron(input_win, COLOR_PAIR(1));
	if (state & GETBIT_READ)
		mvwprintw(input_win, 3, 59, "read     ");
	if (state & GETBIT_RECV)
		mvwprintw(input_win, 3, 70, "receive ");
	else if (state & GETBIT_XMIT)
		mvwprintw(input_win, 3, 70, "transmit");
	else if (state & GETBIT_RND)
		mvwprintw(input_win, 3, 70, "random  ");
	else if (state & GETBIT_IO)
		mvwprintw(input_win, 3, 70, "IO      ");
	else {
		wattron(input_win, COLOR_PAIR(2));
		mvwprintw(input_win, 3, 70, "OK      ");
		wattroff(input_win, COLOR_PAIR(2));
	}
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
		}
	} else
		bitpos++;
	if (bitpos == sizeof(buffer)) {
		state |= GETBIT_TOOLONG;
		bitpos = 0;
		if (fromfile == 1)
			printf(" L");
		else {
			wattron(input_win, COLOR_PAIR(1));
			mvwprintw(input_win, 3, 59, "no minute");
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

void
draw_input_window(void)
{
	mvwprintw(input_win, 0, 0, "new");
	mvwprintw(input_win, 2, 0, "bit  act total          realfreq   b0"
	    "  b20  max1 increment state      radio");
	wrefresh(input_win);
}

int
switch_logfile(WINDOW *win, char **logfilename)
{
	int res;
	char *old_logfilename;

	if (*logfilename == NULL)
		*logfilename = strdup("");
	old_logfilename = strdup(*logfilename);
	free(*logfilename);
	*logfilename = strdup(get_keybuf());
	if (!strcmp(*logfilename, ".")) {
		free(*logfilename);
		*logfilename = strdup(old_logfilename);
	}

	if (strcmp(old_logfilename, *logfilename)) {
		if (strlen(old_logfilename) > 0) {
			if (fclose(logfile) == EOF) {
				statusbar(win, bitpos,
				    "Error closing old log file");
				return errno;
			}
		}
		if (strlen(*logfilename) > 0) {
			if ((res = write_new_logfile(*logfilename)) != 0) {
				statusbar(win, bitpos, strerror(res));
				return res;
			}
		}
	}
	free(old_logfilename);
	return 0;
}

int
write_new_logfile(char *logfilename)
{
	logfile = fopen(logfilename, "a");
	if (logfile == NULL)
		return errno;
	fprintf(logfile, "\n--new log--\n\n");
	return 0;
}
