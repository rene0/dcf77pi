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

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/param.h>

#if defined(__FreeBSD__)
#  if __FreeBSD_version >= 900022
#    include <sys/gpio.h>
#  else
#    define NOLIVE 1
#  endif
#elif defined(__NetBSD__)
#  warning NetBSD, GPIO support not yet implemented
#  define NOLIVE 1
#elif defined(__linux__)
#  include <sys/types.h>
#elif defined(__APPLE__) && (defined(__OSX__) || defined(__MACH__))
#  warning MacOS, GPIO support available but no port for Rapberry Pi
#  define NOLIVE 1
#elif defined(__CYGWIN__)
#  warning Cygwin, GPIO support not yet implemented
#  define NOLIVE 1
#elif defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#  error Use Cygwin to use this software on Windows
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
struct bitinfo bit;

int
init_hardware(unsigned int pin_nr)
{
#if defined(__FreeBSD__) && !defined(NOLIVE)
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
	res = snprintf(buf, sizeof(buf), "%u", pin_nr);
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
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/direction",
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
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/value",
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
	(void)memset(buffer, '\0', sizeof(buffer));
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
#if defined(NOLIVE)
	printf("No GPIO interface available, disabling live decoding\n");
	cleanup();
	return -1;
#else
	/* fill hardware structure and initialize hardware */
	hw.pin = strtol(get_config_value("pin"), NULL, 10);
	hw.active_high = strtol(get_config_value("activehigh"), NULL, 10);
	if (hw.active_high > 1) {
		printf("hw.active_high must either be 0 or 1\n");
		cleanup();
		return -1;
	}
	hw.freq = strtol(get_config_value("freq"), NULL, 10);
	if (hw.freq < 10 || hw.freq > 666666 || (hw.freq & 1) == 1) {
		printf("hw.freq must be an even number between 10 and 666666 inclusive\n");
		cleanup();
		return -1;
	}
	res = init_hardware(hw.pin);
	if (res < 0) {
		cleanup();
		return res;
	}
#endif
	return 0;
}

void
cleanup(void)
{
	if (fd > 0 && close(fd) == -1)
#if defined(__FreeBSD__)
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
	free(bit.signal);
}

uint8_t
get_pulse(void)
{
	uint8_t tmpch = 0;
	int count = 0;
#if defined(__FreeBSD__) && !defined(NOLIVE)
	struct gpio_req req;

	req.gp_pin = hw.pin;
	count = ioctl(fd, GPIOGET, &req);
	tmpch = (req.gp_value == GPIO_PIN_HIGH) ? 1 : 0;
	if (count < 0)
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

/*
 * Clear previous flags, except GETBIT_TOOLONG to be able
 * to determine if this flag can be cleared again.
 */
void
set_new_state(void)
{
	state = (state & GETBIT_TOOLONG) ? GETBIT_TOOLONG : 0;
}

void
reset_frequency(void)
{
	if (logfile != NULL)
		fprintf(logfile, bit.realfreq <= hw.freq * 500000 ?  "<" :
		    bit.realfreq >= hw.freq * 1500000 ? ">" : "");
	bit.realfreq = hw.freq * 1000000;
	bit.freq_reset = 1;
}

void
reset_bitlen(void)
{
	if (logfile != NULL)
		fprintf(logfile, "!");
	bit.bit0 = bit.realfreq / 10;
	bit.bit20 = bit.realfreq / 5;
	bit.bitlen_reset = 1;
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

	char outch;
	int newminute;
	uint8_t p, stv = 1;
	struct timespec slp;
	uint32_t sec2;
	int64_t a, y = 1000000000;
	static int init = 1;
	int is_eom = state & GETBIT_EOM;

	bit.freq_reset = 0;
	bit.bitlen_reset = 0;

	set_new_state();

	/*
	 * One period is either 1000 ms or 2000 ms long (normal or padding for
	 * last). The active part is either 100 ms ('0') or 200 ms ('1') long.
	 * The maximum allowed values as percentage of the second length are
	 * secified as half the value and the whole value of the lengths of
	 * bit 0 and bit 20 respectively.
	 *
	 *  ~A > 3/2 * realfreq: value |= GETBIT_EOM
	 *  ~A > 5/2 * realfreq: timeout
	 */

	if (init == 1) {
		bit.realfreq = hw.freq * 1000000;
		bit.bit0 = bit.realfreq / 10;
		bit.bit20 = bit.realfreq / 5;
		bit.signal = malloc(hw.freq / 2);
	}
	sec2 = 1000000000 / (hw.freq * hw.freq);
	/*
	 * Set up filter, reach 50% after realfreq/20 samples (i.e. 50 ms)
	 */
	a = 1000000000 - 1000000000 * exp2(-2e7 / bit.realfreq);
	bit.tlow = -1;
	bit.tlast0 = -1;

	for (bit.t = 0; ; bit.t++) {
		p = get_pulse();
		if (p == GETBIT_IO) {
			state |= GETBIT_IO;
			outch = '*';
			goto report;
		}
		if ((bit.t & 7) == 0)
			bit.signal[bit.t / 8] = 0;
			/* clear data from previous second */
		bit.signal[bit.t / 8] |= p << (bit.t & 7);

		if (y >= 0 && y < a / 2)
			bit.tlast0 = bit.t;
		y += a * (p * 1000000000 - y) / 1000000000;

		/*
		 * Prevent algorithm collapse during thunderstorms
		 * or scheduler abuse
		 */
		if (bit.realfreq <= hw.freq * 500000 ||
		    bit.realfreq >= hw.freq * 1500000)
			reset_frequency();

		if (bit.t > bit.realfreq * 2500000) {
			bit.realfreq = bit.realfreq +
			    (bit.t * 2500000 - bit.realfreq) / 20;
			a = 1000000000 - 1000000000 * exp2(-2e7 / bit.realfreq);
			if (bit.tlow * 100 / bit.t < 1) {
				state |= GETBIT_RECV;
				outch = 'r';
			} else if (bit.tlow * 100 / bit.t >= 99) {
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
		if (y < 500000000 && stv == 1) {
			y = 0;
			stv = 0;
			bit.tlow = bit.t; /* end of high part of second */
		}
		if (y > 500000000 && stv == 0) {
			y = 1000000000;
			stv = 1;

			newminute = bit.t * 2000000 > bit.realfreq * 3;
			if (init == 1)
				init = 2;
			else {
				if (newminute)
					bit.realfreq = bit.realfreq + (bit.t *
					    500000 - bit.realfreq) / 20;
				else
					bit.realfreq = bit.realfreq + (bit.t *
					    1000000 - bit.realfreq) / 20;
				a = 1000000000 - 1000000000 *
				    exp2(-2e7 / bit.realfreq);
			}

			if (newminute) {
				/*
				 * Reset the frequency and the EOM flag if two
				 * consecutive EOM markers come in, which means
				 * something is wrong.
				 */
				if (is_eom) {
					state &= ~GETBIT_EOM;
					reset_frequency();
				} else
					state |= GETBIT_EOM;
			}
			break; /* start of new second */
		}
		slp.tv_sec = 0;
		slp.tv_nsec = sec2 * bit.realfreq / 1000000;
		while (nanosleep(&slp, &slp))
			;
	}

	if (2 * bit.realfreq * bit.tlow * (1 + newminute) <
	    (bit.bit0 + bit.bit20) * bit.t) {
		/* zero bit, ~100 ms active signal */
		outch = '0';
		buffer[bitpos] = 0;
	} else if (bit.realfreq * bit.tlow * (1 + newminute) <
	    (bit.bit0 + bit.bit20) * bit.t) {
		/* one bit, ~200 ms active signal */
		state |= GETBIT_ONE;
		outch = '1';
		buffer[bitpos] = 1;
	} else {
		/* bad radio signal, retain old value */
		state |= GETBIT_READ;
		outch = '_';
		/* force bit 20 to be 1 to recover from too low b20 value */
		if (bitpos == 20) {
			state |= GETBIT_ONE;
			buffer[bitpos] = 1;
		}
	}
	if (init == 2)
		init = 0;
	else if ((state & (GETBIT_RND | GETBIT_XMIT |
	    GETBIT_RECV | GETBIT_EOM | GETBIT_TOOLONG)) == 0) {
		if (bitpos == 0 && buffer[0] == 0 && (state & GETBIT_READ) == 0)
			bit.bit0 = bit.bit0 +
			    (bit.tlow * 1000000 - bit.bit0) / 2;
		if (bitpos == 20 && buffer[20] == 1)
			bit.bit20 = bit.bit20 +
			    (bit.tlow * 1000000 - bit.bit20) / 2;
		/* During a thunderstorm the value of bit20 might underflow */
		if (bit.bit20 < bit.bit0)
			reset_bitlen();
	}
report:
	if (logfile != NULL)
		fprintf(logfile, "%c%s", outch, state & GETBIT_EOM ? "\n" : "");
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

	set_new_state();

	bit.realfreq = 1000000000; /* fake realfreq for mainloop() */
	bit.t = 1000;

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
			bit.t = 2000;
			break;
		case 'x':
			state |= GETBIT_XMIT;
			valid = 1;
			bit.t = 2500;
			break;
		case 'r':
			state |= GETBIT_RECV;
			valid = 1;
			bit.t = 2500;
			break;
		case '#':
			state |= GETBIT_RND;
			valid = 1;
			bit.t = 2500;
			break;
		case '*':
			state |= GETBIT_IO;
			valid = 1;
			bit.t = 0;
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
		bit.t = 2000;
		/* Check for B\r\n or B\n\r */
		TRYCHAR
	}
	return state;
}

int
is_space_bit(int bitpos)
{
	return (bitpos == 1 || bitpos == 15 || bitpos == 16 || bitpos == 19 ||
	    bitpos == 20 || bitpos == 21 || bitpos == 28 || bitpos == 29 ||
	    bitpos == 35 || bitpos == 36 || bitpos == 42 || bitpos == 45 ||
	    bitpos == 50 || bitpos == 58 || bitpos == 59 || bitpos == 60);
}

uint16_t
next_bit(void)
{
	bitpos = (state & GETBIT_EOM) ? 0 : bitpos + 1;
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

int
write_new_logfile(char *logfilename)
{
	logfile = fopen(logfilename, "a");
	if (logfile == NULL)
		return errno;
	fprintf(logfile, "\n--new log--\n\n");
	return 0;
}

int
close_logfile(void)
{
	int f;
	f = fclose(logfile);
	return (f == EOF) ? errno : 0;
}

struct bitinfo *
get_bitinfo(void)
{
	return &bit;
}
