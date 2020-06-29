// Copyright 2013-2020 René Ladan and Udo Klein and "JsBergbau"
// SPDX-License-Identifier: BSD-2-Clause

#include "input.h"

#include "json_object.h"

#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#  include <sys/param.h>
#  if __FreeBSD_version >= 900022
#    include <sys/gpio.h>
#    include <sys/ioccom.h>
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

static int bitpos;              /* second */
static unsigned dec_bp;         /* bitpos decrease in file mode */
static FILE *logfile;           /* auto-appended in live mode */
static int fd;                  /* gpio file */
static struct hardware hw;
static struct bitinfo bit;
static unsigned acc_minlen;
static float cutoff;
static unsigned filemode = 0;   /* 0 = no file, 1 = input, 2 = output */

int buffer[BUFLEN];      /* wrap after BUFLEN positions */

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
	int tmpch;
#if defined(NOLIVE)
	tmpch = 2;
#else
	int count = 0;
#if defined(__FreeBSD__)
	struct gpio_req req;

	req.gp_pin = hw.pin;
	count = ioctl(fd, GPIOGET, &req);
	tmpch = (req.gp_value == GPIO_PIN_HIGH) ? 1 : 0;
	if (count < 0) {
#elif defined(__linux__)
	count = read(fd, &tmpch, 1);
	tmpch -= '0';
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1 || count != 1) {
#endif
		return 2; /* hardware or virtual FS failure? */
	}
	if (!hw.active_high) {
		tmpch = 1 - tmpch;
	}
#endif
	return tmpch;
}

struct GB_result
set_new_state(struct GB_result in_gbr)
{
	struct GB_result gbr;

	memcpy(&gbr, &in_gbr, sizeof(struct GB_result));
	if (!in_gbr.skip) {
		cutoff = -1;
	}
	gbr.bad_io = false;
	gbr.bitval = ebv_none;
	if (in_gbr.marker != emark_toolong && in_gbr.marker != emark_late) {
		gbr.marker = emark_none;
	}
	gbr.hwstat = ehw_ok;
	gbr.done = false;
	gbr.skip = false;

	return gbr;
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
	static struct GB_result gbr;
	static int oldinch;
	static bool read_acc_minlen;
	int inch;

	gbr = set_new_state(gbr);

	inch = skip_invalid();
	/*
	 * bit.t is set to fake value for compatibility with old log files not
	 * storing acc_minlen values or to increase time when mainloop*() splits
	 * too long minutes.
	 */

	switch (inch) {
	case EOF:
		gbr.done = true;
		return gbr;
	case '0':
	case '1':
		buffer[bitpos] = inch - (int)'0';
		gbr.bitval = (inch == (int)'0') ? ebv_0 : ebv_1;
		bit.t = 1000;
		break;
	case '\n':
		/*
		 * Skip multiple consecutive EOM markers, these are made
		 * impossible by the reset_minlen() invocation in
		 * mainloop_live()
		 */
		gbr.skip = true;
		if (oldinch != '\n') {
			bit.t = read_acc_minlen ? 0 : 1000;
			read_acc_minlen = false;
			/*
			 * The marker checks must be inside the oldinch
			 * check to prevent spurious emin_short errors.
			 */
			if (gbr.marker == emark_none) {
				gbr.marker = emark_minute;
			} else if (gbr.marker == emark_toolong) {
				gbr.marker = emark_late;
			}
		} else {
			bit.t = 0;
		}
		break;
	case 'x':
		gbr.hwstat = ehw_transmit;
		bit.t = 2500;
		break;
	case 'r':
		gbr.hwstat = ehw_receive;
		bit.t = 2500;
		break;
	case '#':
		gbr.hwstat = ehw_random;
		bit.t = 2500;
		break;
	case '*':
		gbr.bad_io = true;
		bit.t = 0;
		break;
	case '_':
		/* retain old value in buffer[bitpos] */
		gbr.bitval = ebv_none;
		bit.t = 1000;
		break;
	case 'a':
		/* acc_minlen, up to 2^32-1 ms */
		gbr.skip = true;
		bit.t = 0;
		if (fscanf(logfile, "%10u", &acc_minlen) != 1) {
			gbr.done = true;
		}
		read_acc_minlen = !gbr.done;
		break;
	case 'c':
		/* cutoff for newminute */
		gbr.skip = true;
		bit.t = 0;
		if (fscanf(logfile, "%6f", &cutoff) != 1) {
			gbr.done = true;
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
		gbr.done = true;
	}
	ungetc(inch, logfile);

	return gbr;
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
next_bit(struct GB_result in_gbr)
{
	struct GB_result gbr;

	memcpy(&gbr, &in_gbr, sizeof(struct GB_result));

	if (dec_bp == 1) {
		bitpos--;
		dec_bp = 2;
	}
	if (in_gbr.marker == emark_minute || in_gbr.marker == emark_late) {
		bitpos = 0;
		dec_bp = 0;
	} else if (!in_gbr.skip) {
		bitpos++;
	}
	if (bitpos == BUFLEN) {
		gbr.marker = emark_toolong;
		bitpos = 0;
		return gbr;
	}
	if (in_gbr.marker == emark_toolong) {
		gbr.marker = emark_none; /* fits again */
	}
	else if (in_gbr.marker == emark_late) {
		gbr.marker = emark_minute; /* cannot happen? */
	}
	return gbr;
}

int
get_bitpos(void)
{
	return bitpos;
}

struct hardware
get_hardware_parameters(void)
{
	return hw;
}

void
*flush_logfile(/*@unused@*/ void *arg)
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
	return pthread_create(&flush_thread, NULL, flush_logfile, NULL);
}

void
write_to_logfile(char chr)
{
	if (logfile != NULL) {
		fprintf(logfile, "%c", chr);
	}
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

float
get_cutoff(void)
{
	return cutoff;
}
