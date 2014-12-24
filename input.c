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
#  define MACOS 1
#elif defined(__CYGWIN__)
#  warning Cygwin, GPIO support not yet implemented
#  define NOLIVE 1
#elif defined(_WIN16) || defined(_WIN32) || defined(_WIN64)
#  error Use Cygwin to use this software on Windows
#else
#  error Unsupported operating system, please send a patch to the author
#endif

uint8_t bitpos;         /* second */
uint8_t buffer[BUFLEN]; /* wrap after BUFLEN positions */
uint16_t state;         /* any errors, or high bit */
FILE *datafile;         /* input file (recorded data) */
FILE *logfile;          /* auto-appended in live mode */
int fd;                 /* gpio file */
struct hardware hw;
struct bitinfo bit;
uint32_t acc_minlen;
uint16_t cutoff;

int
set_mode_file(const char * const infilename)
{
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
#if !defined(NOLIVE)
#if defined(__FreeBSD__)
	struct gpio_pin pin;
#elif defined(__linux__)
	char buf[64];
#endif
#endif

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
	if (hw.freq < 10 || hw.freq > 155000 || (hw.freq & 1) == 1) {
		printf("hw.freq must be an even number between 10 and 155000 inclusive\n");
		cleanup();
		return -1;
	}
#if defined(__FreeBSD__)
	fd = open("/dev/gpioc0", O_RDONLY);
	if (fd < 0) {
		perror("open (/dev/gpioc0)");
		cleanup();
		return errno;
	}

	pin.gp_pin = hw.pin;
	if (ioctl(fd, GPIOGETCONFIG, &pin) < 0) {
		perror("ioctl(GPIOGETCONFIG)");
		cleanup();
		return errno;
	}
#elif defined(__linux__)
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd < 0) {
		perror("open (/sys/class/gpio/export)");
		cleanup();
		return errno;
	}
	res = snprintf(buf, sizeof(buf), "%u", hw.pin);
	if (res < 0 || res > sizeof(buf)-1) {
		printf("hw.pin too high? (%i)\n", res);
		cleanup();
		return -1;
	}
	if (write(fd, buf, res) < 0) {
		perror("write(export)");
		cleanup();
		if (errno != EBUSY)
			return errno; /* EBUSY -> pin already exported ? */
	}
	if (close(fd) == -1) {
		perror("close(export)");
		cleanup();
		return errno;
	}
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/direction",
	    hw.pin);
	if (res < 0 || res > sizeof(buf)-1) {
		printf("hw.pin too high? (%i)\n", res);
		cleanup();
		return -1;
	}
	fd = open(buf, O_WRONLY);
	if (fd < 0) {
		perror("open (direction)");
		cleanup();
		return errno;
	}
	if (write(fd, "in", 3) < 0) {
		perror("write(in)");
		cleanup();
		return errno;
	}
	if (close(fd) == -1) {
		perror("close(direction)");
		cleanup();
		return errno;
	}
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/value",
	    hw.pin);
	if (res < 0 || res > sizeof(buf)-1) {
		printf("hw.pin too high? (%i)\n", res);
		cleanup();
		return -1;
	}
	fd = open(buf, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open (value)");
		cleanup();
		return errno;
	}
#endif
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
#if !defined(NOLIVE)
	int count = 0;
#if defined(__FreeBSD__)
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
#endif
		return GETBIT_IO; /* hardware failure? */

	if (!hw.active_high)
		tmpch = 1 - tmpch;
	return tmpch;
}

/*
 * Clear the cutoff value and the previous flags, except GETBIT_TOOLONG to be
 * able to determine if this flag can be cleared again.
 */
void
set_new_state(void)
{
	cutoff = 0xffff;
	state = (state & GETBIT_TOOLONG) ? GETBIT_TOOLONG : 0;
}

void
reset_frequency(void)
{
	if (logfile != NULL)
		fprintf(logfile, bit.realfreq <= hw.freq * 500000 ?  "<" :
		    bit.realfreq >= hw.freq * 1500000 ? ">" : "");
	bit.realfreq = hw.freq * 1000000;
	bit.freq_reset = true;
}

void
reset_bitlen(void)
{
	if (logfile != NULL)
		fprintf(logfile, "!");
	bit.bit0 = bit.realfreq / 10;
	bit.bit20 = bit.realfreq / 5;
	bit.bitlen_reset = true;
}

/*
 * The bits are decoded from the signal using an exponential low-pass filter in
 * conjunction with a Schmitt trigger. The idea and the initial implementation
 * for this come from Udo Klein, with permission.
 * http://blog.blinkenlight.net/experiments/dcf77/binary-clock/#comment-5916
 */
uint16_t
get_bit_live(void)
{
	char outch;
	bool newminute;
	uint8_t p, stv = 1;
	struct timespec slp;
#if !defined(MACOS)
	struct timespec tp0, tp1;
#endif
	uint32_t sec2;
	int64_t a, y = 1000000000;
	int64_t twait;
	static int init_bit = 2;
	bool is_eom = state & GETBIT_EOM;

	bit.freq_reset = false;
	bit.bitlen_reset = false;

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

	if (init_bit == 2) {
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
#if !defined(MACOS)
		(void)clock_gettime(CLOCK_MONOTONIC, &tp0);
#endif
		p = get_pulse();
		if (p == GETBIT_IO) {
			state |= GETBIT_IO;
			outch = '*';
			goto report;
		}
		if ((bit.t & 7) == 0)
			bit.signal[bit.t / 8] = 0;
			/* clear data from previous second */
		bit.signal[bit.t / 8] |= p << (uint8_t)(bit.t & 7);

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
			bit.realfreq += ((int64_t)
			    (bit.t * 2500000 - bit.realfreq) / 20);
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
			if (init_bit == 2)
				init_bit--;
			else {
				if (newminute)
					bit.realfreq += ((int64_t)(bit.t *
					    500000 - bit.realfreq) / 20);
				else
					bit.realfreq += ((int64_t)(bit.t *
					    1000000 - bit.realfreq) / 20);
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
		twait = sec2 * bit.realfreq / 1000000;
#if !defined(MACOS)
		(void)clock_gettime(CLOCK_MONOTONIC, &tp1);
		twait = twait - (tp1.tv_sec - tp0.tv_sec) * 1000000000 -
		   (tp1.tv_nsec - tp0.tv_nsec);
#endif
		slp.tv_sec = twait / 1000000000;
		slp.tv_nsec = twait % 1000000000;
		while (twait > 0 && nanosleep(&slp, &slp))
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
	if (init_bit == 1)
		init_bit--;
	else if ((state & (GETBIT_RND | GETBIT_XMIT |
	    GETBIT_RECV | GETBIT_EOM | GETBIT_TOOLONG)) == 0) {
		if (bitpos == 0 && buffer[0] == 0 && (state & GETBIT_READ) == 0)
			bit.bit0 += ((int64_t)
			    (bit.tlow * 1000000 - bit.bit0) / 2);
		if (bitpos == 20 && buffer[20] == 1)
			bit.bit20 += ((int64_t)
			    (bit.tlow * 1000000 - bit.bit20) / 2);
		/* During a thunderstorm the value of bit20 might underflow */
		if (bit.bit20 < bit.bit0)
			reset_bitlen();
	}
report:
	acc_minlen += 1000000 * bit.t / (bit.realfreq / 1000);
	if (logfile != NULL) {
		fprintf(logfile, "%c", outch);
		if (state & GETBIT_EOM)
			fprintf(logfile, "a%uc%6.4f\n", acc_minlen,
			    (bit.t * 1e6) / bit.realfreq);
	}
	if (state & GETBIT_EOM)
		cutoff = (uint16_t)(bit.t * 1000000 / (bit.realfreq / 10000));
	return state;
}

#define TRYCHAR \
	oldinch = inch; \
	inch = getc(datafile); \
	if (inch == EOF) \
		state |= GETBIT_EOD; \
	if (inch == 'a' || inch == 'c') \
		state |= GETBIT_SKIPNEXT; \
	if ((inch != '\r' && inch != '\n') || inch == oldinch) { \
		if (ungetc(inch, datafile) == EOF) /* EOF remains, IO error */\
			state |= GETBIT_EOD; \
	}

#define READVALUE(COND) \
do { \
	state &= ~GETBIT_SKIPNEXT; \
	state |= GETBIT_SKIP; \
	valid = true; \
	bit.t = 0; \
	if (COND) \
		state |= GETBIT_EOD; \
} while (0)

uint16_t
get_bit_file(void)
{
	int oldinch, inch;
	bool valid = false;
	static bool read_acc_minlen;
	char co[6];

	set_new_state();

	/*
	 * bit.realfreq and bit.t are set to fake value for compatibility with
	 * old log files not storing acc_minlen values or to increase time
	 * when mainloop() splits too long minutes
	 */
	bit.realfreq = 1000 * 1000000;

	while (!valid) {
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
			valid = true;
			bit.t = 1000;
			break;
		case '\r':
		case '\n':
			/*
			 * Skip multiple consecutive EOM markers,
			 * these are made impossible by the reset_minlen()
			 * invocation in get_bit_live()
			 */
			break;
		case 'x':
			state |= GETBIT_XMIT;
			valid = true;
			bit.t = 2500;
			break;
		case 'r':
			state |= GETBIT_RECV;
			valid = true;
			bit.t = 2500;
			break;
		case '#':
			state |= GETBIT_RND;
			valid = true;
			bit.t = 2500;
			break;
		case '*':
			state |= GETBIT_IO;
			valid = true;
			bit.t = 0;
			break;
		case '_':
			/* retain old value in buffer[bitpos] */
			state |= GETBIT_READ;
			valid = true;
			bit.t = 1000;
			break;
		case 'a':
			/* acc_minlen */
			READVALUE(fscanf(datafile, "%u", &acc_minlen) != 1);
			read_acc_minlen = !(state & GETBIT_EOD);
			break;
		case 'c':
			/* cutoff for newminute */
			READVALUE(fscanf(datafile, "%6c", co) != 1);
			if (!(state & GETBIT_EOD) & (co[1] == '.'))
				cutoff = (co[0] - '0') * 10000 +
				    (uint16_t)strtol(co + 2, (char **)NULL, 10);
			break;
		default:
			break;
		}
	}
	/* Only allow \r , \n , \r\n , and \n\r as single EOM markers */
	TRYCHAR else {
		state |= GETBIT_EOM;
		if (!read_acc_minlen)
			bit.t += 1000;
		else
			read_acc_minlen = false;
		/* Check for \r\n or \n\r */
		TRYCHAR
	}
	if (!read_acc_minlen)
		acc_minlen += 1000000 * bit.t / (bit.realfreq / 1000);

	return state;
}

bool
is_space_bit(uint8_t bitpos)
{
	return (bitpos == 1 || bitpos == 15 || bitpos == 16 || bitpos == 19 ||
	    bitpos == 20 || bitpos == 21 || bitpos == 28 || bitpos == 29 ||
	    bitpos == 35 || bitpos == 36 || bitpos == 42 || bitpos == 45 ||
	    bitpos == 50 || bitpos == 58 || bitpos == 59 || bitpos == 60);
}

uint16_t
next_bit(void)
{
	bitpos = (state & GETBIT_EOM) ? 0 : (state & GETBIT_SKIPNEXT) ?
	    bitpos : bitpos + 1;
	if (bitpos == BUFLEN) {
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

const uint8_t * const
get_buffer(void)
{
	return buffer;
}

const struct hardware * const
get_hardware_parameters(void)
{
	return &hw;
}

int
write_new_logfile(const char * const logfilename)
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

const struct bitinfo * const
get_bitinfo(void)
{
	return &bit;
}

uint32_t
get_acc_minlen(void)
{
	return acc_minlen;
}

void
reset_acc_minlen(void)
{
	acc_minlen = 0;
}

uint16_t
get_cutoff(void)
{
	return cutoff;
}
