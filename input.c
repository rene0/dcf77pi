// Copyright 2013-2018 René Ladan and Udo Klein and "JsBergbau"
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"

#include <json.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/time.h>

#if defined(__FreeBSD__)
#  include <sys/param.h>
#  if __FreeBSD_version >= 900022
#    include <sys/gpio.h>
#    include <sys/ioctl.h>
#  else
#    define NOLIVE 1
#  endif
#elif defined(__NetBSD__)
#  warning NetBSD, GPIO support not yet implemented
#  define NOLIVE 1
#elif defined(__linux__)
/* NOP */
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

/** maximum number of bits in a minute */
#define BUFLEN 60

static int bitpos;              /* second */
static unsigned dec_bp;         /* bitpos decrease in file mode */
static int buffer[BUFLEN];      /* wrap after BUFLEN positions */
static FILE *logfile;           /* auto-appended in live mode */
static int fd;                  /* gpio file */
static struct hardware hw;
static struct bitinfo bit;
static unsigned acc_minlen;
static int cutoff;
static int pulse;
static struct GB_result gb_res;
static unsigned filemode = 0;   /* 0 = no file, 1 = input, 2 = output */

int
set_mode_file(const char * const infilename)
{
	if (filemode == 1) {
		fprintf(stderr, "Already initialized to live mode.\n");
		cleanup();
		return -1;
	}
	if (infilename == NULL) {
		fprintf(stderr, "infilename is NULL\n");
		return -1;
	}
	logfile = fopen(infilename, "r");
	if (logfile == NULL) {
		perror("fopen(logfile)");
		return errno;
	}
	filemode = 2;
	return 0;
}

void
obtain_pulse(/*@unused@*/ int sig)
{
	pulse = 2;
#if !defined(NOLIVE)
	int tmpch;
	int count = 0;
#if defined(__FreeBSD__)
	struct gpio_req req;

	req.gp_pin = hw.pin;
	count = ioctl(fd, GPIOGET, &req);
	tmpch = (req.gp_value == GPIO_PIN_HIGH) ? 1 : 0;
	if (count < 0)
#elif defined(__linux__)
	count = read(fd, &tmpch, 1);
	tmpch -= '0';
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1 || count != 1)
#endif
		pulse = 2; /* hardware or virtual FS failure? */
	else if (hw.active_high)
		pulse = tmpch;
	else
		pulse = 1 - tmpch;
#endif
}

int
set_mode_live(struct json_object *config)
{
#if defined(NOLIVE)
	fprintf(stderr,
	    "No GPIO interface available, disabling live decoding\n");
	cleanup();
	return -1;
#else
#if defined(__FreeBSD__)
	struct gpio_pin pin;
#endif
	struct itimerval itv;
	struct sigaction sigact;
	char buf[64];
	struct json_object *value;
	int res;

	if (filemode == 2) {
		fprintf(stderr, "Already initialized to file mode.\n");
		cleanup();
		return -1;
	}
	/* fill hardware structure and initialize hardware */
	if (json_object_object_get_ex(config, "pin", &value)) {
		hw.pin = (unsigned)json_object_get_int(value);
	} else {
		fprintf(stderr, "Key 'pin' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	if (json_object_object_get_ex(config, "activehigh", &value)) {
		hw.active_high = (bool)json_object_get_boolean(value);
	} else {
		fprintf(stderr, "Key 'activehigh' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	if (json_object_object_get_ex(config, "freq", &value)) {
		hw.freq = (unsigned)json_object_get_int(value);
	} else {
		fprintf(stderr, "Key 'freq' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	if (hw.freq < 10 || hw.freq > 155000 || (hw.freq & 1) == 1) {
		fprintf(stderr, "hw.freq must be an even number between 10 and"
		    "155000 inclusive\n");
		cleanup();
		return EX_DATAERR;
	}
	bit.signal = malloc(hw.freq / 2);
#if defined(__FreeBSD__)
	if (json_object_object_get_ex(config, "iodev", &value)) {
		hw.iodev = (unsigned)json_object_get_int(value);
	} else {
		fprintf(stderr, "Key 'iodev' not found\n");
		cleanup();
		return EX_DATAERR;
	}
	res = snprintf(buf, sizeof(buf), "/dev/gpioc%u", hw.iodev);
	if (res < 0 || res >= sizeof(buf)) {
		fprintf(stderr, "hw.iodev too high? (%i)\n", res);
		cleanup();
		return EX_DATAERR;
	}
	fd = open(buf, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "open %s: ", buf);
		perror(NULL);
		cleanup();
		return errno;
	}

	pin.gp_pin = hw.pin;
	pin.gp_flags = GPIO_PIN_INPUT;
	if (ioctl(fd, GPIOSETCONFIG, &pin) < 0) {
		perror("ioctl(GPIOSETCONFIG)");
		cleanup();
		return errno;
	}
#elif defined(__linux__)
	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (fd < 0) {
		perror("open(/sys/class/gpio/export)");
		cleanup();
		return errno;
	}
	res = snprintf(buf, sizeof(buf), "%u", hw.pin);
	if (res < 0 || res >= sizeof(buf)) {
		fprintf(stderr, "hw.pin too high? (%i)\n", res);
		cleanup();
		return EX_DATAERR;
	}
	if (write(fd, buf, res) < 0) {
		if (errno != EBUSY) {
			perror("write(export)");
			cleanup();
			return errno; /* EBUSY -> pin already exported ? */
		}
	}
	if (close(fd) == -1) {
		perror("close(export)");
		cleanup();
		return errno;
	}
	res = snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%u/direction",
	    hw.pin);
	if (res < 0 || res >= sizeof(buf)) {
		fprintf(stderr, "hw.pin too high? (%i)\n", res);
		cleanup();
		return EX_DATAERR;
	}
	fd = open(buf, O_RDWR);
	if (fd < 0) {
		perror("open(direction)");
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
	if (res < 0 || res >= sizeof(buf)) {
		fprintf(stderr, "hw.pin too high? (%i)\n", res);
		cleanup();
		return EX_DATAERR;
	}
	fd = open(buf, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open(value)");
		cleanup();
		return errno;
	}
#endif
	/* set up the signal handler */
	sigact.sa_handler = obtain_pulse;
	(void)sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = 0;
	sigaction(SIGALRM, &sigact, (struct sigaction *)NULL);
	/* set up the timer */
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 1000000 / hw.freq;
	memcpy(&itv.it_value, &itv.it_interval, sizeof(struct timeval));
	(void)setitimer(ITIMER_REAL, &itv, NULL);

	filemode = 1;
	return 0;
#endif
}

void
cleanup(void)
{
	if (fd > 0 && close(fd) == -1) {
#if defined(__FreeBSD__)
		perror("close(/dev/gpioc*)");
#elif defined(__linux__)
		perror("close(/sys/class/gpio/*)");
#endif
	}
	fd = 0;
	if (logfile != NULL) {
		if (fclose(logfile) == EOF) {
			perror("fclose(logfile)");
		} else {
			logfile = NULL;
		}
	}
	free(bit.signal);
}

int
get_pulse(void)
{
	return pulse;
}

/*
 * Clear the cutoff value and the state values, except emark_toolong and
 * emark_late to be able to determine if this flag can be cleared again.
 */
static void
set_new_state(void)
{
	if (!gb_res.skip) {
		cutoff = -1;
	}
	gb_res.bad_io = false;
	gb_res.bitval = ebv_none;
	if (gb_res.marker != emark_toolong && gb_res.marker != emark_late) {
		gb_res.marker = emark_none;
	}
	gb_res.hwstat = ehw_ok;
	gb_res.done = false;
	gb_res.skip = false;
}

static void
reset_frequency(void)
{
	if (logfile != NULL) {
		fprintf(logfile, "%s",
		    bit.realfreq <= hw.freq * 500000 ? "<" :
		    bit.realfreq > hw.freq * 1000000 ? ">" : "");
	}
	bit.realfreq = hw.freq * 1000000;
	bit.freq_reset = true;
}

static void
reset_bitlen(void)
{
	if (logfile != NULL) {
		fprintf(logfile, "!");
	}
	bit.bit0 = bit.realfreq / 10;
	bit.bit20 = bit.realfreq / 5;
	bit.bitlen_reset = true;
}

/*
 * The bits are decoded from the signal using an exponential low-pass filter
 * in conjunction with a Schmitt trigger. The idea and the initial
 * implementation for this come from Udo Klein, with permission.
 * http://blog.blinkenlight.net/experiments/dcf77/binary-clock/#comment-5916
 */
struct GB_result
get_bit_live(void)
{
	char outch = '?';
	bool adj_freq = true;
	bool newminute = false;
	unsigned stv = 1;
	long long a, y = 1000000000;
	static int init_bit = 2;
	bool is_eom = gb_res.marker == emark_minute ||
	    gb_res.marker == emark_late;

	bit.freq_reset = false;
	bit.bitlen_reset = false;

	set_new_state();

	/*
	 * One period is either 1000 ms or 2000 ms long (normal or padding for
	 * last). The active part is either 100 ms ('0') or 200 ms ('1') long.
	 * The maximum allowed values as percentage of the second length are
	 * specified as half the value and the whole value of the lengths of
	 * bit 0 and bit 20 respectively.
	 *
	 * ~A > 3/2 * realfreq: end-of-minute
	 * ~A > 5/2 * realfreq: timeout
	 */

	if (init_bit == 2) {
		bit.realfreq = hw.freq * 1000000;
		bit.bit0 = bit.realfreq / 10;
		bit.bit20 = bit.realfreq / 5;
	}
	/* Set up filter, reach 50% after hw.freq/20 samples (i.e. 50 ms) */
	a = 1000000000 - (long long)(1000000000 * exp2(-20.0 / hw.freq));
	bit.tlow = -1;
	bit.tlast0 = -1;

	for (bit.t = 0; bit.t < hw.freq * 4; bit.t++) {
		if (pulse == 2) {
			gb_res.bad_io = true;
			outch = '*';
			break;
		}
		if (bit.signal != NULL) {
			if ((bit.t & 7) == 0) {
				bit.signal[bit.t / 8] = 0;
				/* clear data from previous second */
			}
			bit.signal[bit.t / 8] |= pulse <<
			    (unsigned char)(bit.t & 7);
		}

		if (y >= 0 && y < a / 2) {
			bit.tlast0 = (int)bit.t;
		}
		y += a * (pulse * 1000000000 - y) / 1000000000;

		/*
		 * Prevent algorithm collapse during thunderstorms or
		 * scheduler abuse
		 */
		if (bit.realfreq <= hw.freq * 500000 ||
		    bit.realfreq > hw.freq * 1000000) {
			reset_frequency();
			adj_freq = false;
		}

		if (bit.t > bit.realfreq * 2500000) {
			if (bit.tlow * 100 / bit.t < 1) {
				gb_res.hwstat = ehw_receive;
				outch = 'r';
			} else if (bit.tlow * 100 / bit.t >= 99) {
				gb_res.hwstat = ehw_transmit;
				outch = 'x';
			} else {
				gb_res.hwstat = ehw_random;
				outch = '#';
			}
			adj_freq = false;
			break; /* timeout */
		}

		/*
		 * Schmitt trigger, maximize value to introduce hysteresis and
		 * to avoid infinite memory.
		 */
		if (y < 500000000 && stv == 1) {
			/* end of high part of second */
			y = 0;
			stv = 0;
			bit.tlow = (int)bit.t;
		}
		if (y > 500000000 && stv == 0) {
			/* end of low part of second */
			y = 1000000000;
			stv = 1;

			newminute = bit.t * 2000000 > bit.realfreq * 3;
			if (init_bit == 2) {
				init_bit--;
			}

			if (newminute) {
				/*
				 * Reset the frequency and the EOM flag if two
				 * consecutive EOM markers come in, which means
				 * something is wrong.
				 */
				if (is_eom) {
					if (gb_res.marker == emark_minute) {
						gb_res.marker = emark_none;
					} else if (gb_res.marker ==
					    emark_late) {
						gb_res.marker = emark_toolong;
					}
					reset_frequency();
					adj_freq = false;
				} else {
					if (gb_res.marker == emark_none) {
						gb_res.marker = emark_minute;
					} else if (gb_res.marker ==
					    emark_toolong) {
						gb_res.marker = emark_late;
					}
				}
			}
			break; /* start of new second */
		}
	}
	if (bit.t >= hw.freq * 4) {
		/* this can actually happen */
		if (gb_res.hwstat == ehw_ok) {
			gb_res.hwstat = ehw_random;
			outch = '#';
		}
		reset_frequency();
		adj_freq = false;
	}

	if (!gb_res.bad_io && gb_res.hwstat == ehw_ok) {
		if (2 * bit.realfreq * bit.tlow * (1 + (newminute ? 1 : 0)) <
		    (bit.bit0 + bit.bit20) * bit.t) {
			/* zero bit, ~100 ms active signal */
			gb_res.bitval = ebv_0;
			outch = '0';
			buffer[bitpos] = 0;
		} else if (bit.realfreq * bit.tlow * (1 + (newminute ? 1 : 0)) <
		    (bit.bit0 + bit.bit20) * bit.t) {
			/* one bit, ~200 ms active signal */
			gb_res.bitval = ebv_1;
			outch = '1';
			buffer[bitpos] = 1;
		} else {
			/* bad radio signal, retain old value */
			gb_res.bitval = ebv_none;
			outch = '_';
			adj_freq = false;
		}
	}

	if (!gb_res.bad_io) {
		if (init_bit == 1) {
			init_bit--;
		} else if (gb_res.hwstat == ehw_ok &&
		    gb_res.marker == emark_none) {
			unsigned long long avg;
			if (bitpos == 0 && gb_res.bitval == ebv_0) {
				bit.bit0 +=
				    ((long long)(bit.tlow * 1000000 -
				    bit.bit0) / 2);
			}
			if (bitpos == 20 && gb_res.bitval == ebv_1) {
				bit.bit20 +=
				    ((long long)(bit.tlow * 1000000 -
				    bit.bit20) / 2);
			}
			/* Force sane values during e.g. a thunderstorm */
			if (2 * bit.bit20 < bit.bit0 * 3 ||
			    bit.bit20 > bit.bit0 * 3) {
				reset_bitlen();
				adj_freq = false;
			}
			avg = (bit.bit20 - bit.bit0) / 2;
			if (bit.bit0 + avg < bit.realfreq / 10 ||
			    bit.bit0 - avg > bit.realfreq / 10) {
				reset_bitlen();
				adj_freq = false;
			}
			if (bit.bit20 + avg < bit.realfreq / 5 ||
			    bit.bit20 - avg > bit.realfreq / 5) {
				reset_bitlen();
				adj_freq = false;
			}
		}
	}
	if (adj_freq) {
		if (newminute) {
			bit.realfreq +=
			    ((long long)(bit.t * 500000 - bit.realfreq) / 20);
		} else {
			bit.realfreq +=
			    ((long long)(bit.t * 1000000 - bit.realfreq) / 20);
		}
	}
	acc_minlen += 1000000 * bit.t / (bit.realfreq / 1000);
	if (logfile != NULL) {
		fprintf(logfile, "%c", outch);
		if (gb_res.marker == emark_minute ||
		    gb_res.marker == emark_late) {
			fprintf(logfile, "a%uc%6.4f\n", acc_minlen,
			    (double)((bit.t * 1e6) / bit.realfreq));
		}
	}
	if (gb_res.marker == emark_minute || gb_res.marker == emark_late) {
		cutoff = bit.t * 1000000 / (bit.realfreq / 10000);
	}
	return gb_res;
}

/* Skip over invalid characters */
static int
skip_invalid(void)
{
	int inch = EOF;

	do {
		int oldinch = inch;
		if (feof(logfile)) {
			break;
		}
		inch = getc(logfile);
		/*
		 * \r\n is implicitly converted because \r is invalid character
		 * \n\r is implicitly converted because \n is found first
		 * \n is OK
		 * convert \r to \n
		 */
		if (oldinch == '\r' && inch != '\n') {
			ungetc(inch, logfile);
			inch = '\n';
		}
	} while (strchr("01\nxr#*_ac", inch) == NULL);
	return inch;
}

struct GB_result
get_bit_file(void)
{
	static int oldinch;
	static bool read_acc_minlen;
	int inch;
	char co[6];

	set_new_state();

	inch = skip_invalid();
	/*
	 * bit.t is set to fake value for compatibility with old log files not
	 * storing acc_minlen values or to increase time when mainloop() splits
	 * too long minutes.
	 */

	switch (inch) {
	case EOF:
		gb_res.done = true;
		return gb_res;
	case '0':
	case '1':
		buffer[bitpos] = inch - (int)'0';
		gb_res.bitval = (inch == (int)'0') ? ebv_0 : ebv_1;
		bit.t = 1000;
		break;
	case '\n':
		/*
		 * Skip multiple consecutive EOM markers, these are made
		 * impossible by the reset_minlen() invocation in
		 * get_bit_live()
		 */
		gb_res.skip = true;
		if (oldinch != '\n') {
			bit.t = read_acc_minlen ? 0 : 1000;
			read_acc_minlen = false;
			/*
			 * The marker checks must be inside the oldinch
			 * check to prevent spurious emin_short errors.
			 */
			if (gb_res.marker == emark_none) {
				gb_res.marker = emark_minute;
			} else if (gb_res.marker == emark_toolong) {
				gb_res.marker = emark_late;
			}
		} else {
			bit.t = 0;
		}
		break;
	case 'x':
		gb_res.hwstat = ehw_transmit;
		bit.t = 2500;
		break;
	case 'r':
		gb_res.hwstat = ehw_receive;
		bit.t = 2500;
		break;
	case '#':
		gb_res.hwstat = ehw_random;
		bit.t = 2500;
		break;
	case '*':
		gb_res.bad_io = true;
		bit.t = 0;
		break;
	case '_':
		/* retain old value in buffer[bitpos] */
		gb_res.bitval = ebv_none;
		bit.t = 1000;
		break;
	case 'a':
		/* acc_minlen, up to 2^32-1 ms */
		gb_res.skip = true;
		bit.t = 0;
		if (fscanf(logfile, "%10u", &acc_minlen) != 1) {
			gb_res.done = true;
		}
		read_acc_minlen = !gb_res.done;
		break;
	case 'c':
		/* cutoff for newminute */
		gb_res.skip = true;
		bit.t = 0;
		if (fscanf(logfile, "%6c", co) != 1) {
			gb_res.done = true;
		}
		if (!gb_res.done && (co[1] == '.')) {
			cutoff = (co[0] - '0') * 10000 +
			    (int)strtol(co + 2, NULL, 10);
		}
		break;
	default:
		break;
	}

	if (!read_acc_minlen) {
		acc_minlen += bit.t;
	}

	/*
	 * Read-ahead 1 character to check if a minute marker is coming. This
	 * prevents emark_toolong or emark_late being set 1 bit early.
	 */
	oldinch = inch;
	inch = skip_invalid();
	if (!feof(logfile)) {
		if (dec_bp == 0 && bitpos > 0 && oldinch != '\n' &&
		    (inch == '\n' || inch == 'a' || inch == 'c')) {
			dec_bp = 1;
		}
	} else {
		gb_res.done = true;
	}
	ungetc(inch, logfile);

	return gb_res;
}

bool
is_space_bit(int bitpos)
{
	return (bitpos == 1 || bitpos == 15 || bitpos == 16 ||
	    bitpos == 19 || bitpos == 20 || bitpos == 21 || bitpos == 28 ||
	    bitpos == 29 || bitpos == 35 || bitpos == 36 || bitpos == 42 ||
	    bitpos == 45 || bitpos == 50 || bitpos == 58 || bitpos == 59 ||
	    bitpos == 60);
}

struct GB_result
next_bit(void)
{
	if (dec_bp == 1) {
		bitpos--;
		dec_bp = 2;
	}
	if (gb_res.marker == emark_minute || gb_res.marker == emark_late) {
		bitpos = 0;
		dec_bp = 0;
	} else if (!gb_res.skip) {
		bitpos++;
	}
	if (bitpos == BUFLEN) {
		gb_res.marker = emark_toolong;
		bitpos = 0;
		return gb_res;
	}
	if (gb_res.marker == emark_toolong) {
		gb_res.marker = emark_none; /* fits again */
	}
	else if (gb_res.marker == emark_late) {
		gb_res.marker = emark_minute; /* cannot happen? */
	}
	return gb_res;
}

int
get_bitpos(void)
{
	return bitpos;
}

const int * const
get_buffer(void)
{
	return buffer;
}

struct hardware
get_hardware_parameters(void)
{
	return hw;
}

void
*flush_logfile()
{
	for (;;)
	{
		fflush(logfile);
		sleep(60);
	}
}

int
append_logfile(const char * const logfilename)
{
	pthread_t flush_thread;

	if (logfilename == NULL) {
		fprintf(stderr, "logfilename is NULL\n");
		return -1;
	}
	logfile = fopen(logfilename, "a");
	if (logfile == NULL) {
		return errno;
	}
	fprintf(logfile, "\n--new log--\n\n");
	pthread_create(&flush_thread, NULL, flush_logfile, NULL);
	return 0;
}

int
close_logfile(void)
{
	int f;

	f = fclose(logfile);
	return (f == EOF) ? errno : 0;
}

struct bitinfo
get_bitinfo(void)
{
	return bit;
}

unsigned
get_acc_minlen(void)
{
	return acc_minlen;
}

void
reset_acc_minlen(void)
{
	acc_minlen = 0;
}

int
get_cutoff(void)
{
	return cutoff;
}
