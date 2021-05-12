// Copyright 2013-2020 René Ladan and "JsBergbau"
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DCF77PI_INPUT_H
#define DCF77PI_INPUT_H

#include <stdbool.h>

struct json_object;

/** maximum number of bits in a minute */
#define BUFLEN 60

/** The current bit buffer */
extern int buffer[BUFLEN];

/** Value of the bit received by radio or log file */
enum eGB_bitvalue {
	/** this bit has value 0 */
	ebv_0,
	/** this bit has value 1 */
	ebv_1,
	/** bit value could not be determined */
	ebv_none
};

/** Indicates if end-of-minute was reached, and related errors */
enum eGB_marker {
	/** normal bit */
	emark_none,
	/** end-of-minute marker */
	emark_minute,
	/** bit buffer would overflow */
	emark_toolong,
	/** late end-of-minute marker, split bits? */
	emark_late
};

/** Radio reception state */
enum eGB_HW {
	/** no reception error */
	ehw_ok,
	/** transmitter error, all positive pulses */
	ehw_transmit,
	/** receiver error, all negative pulses */
	ehw_receive,
	/** random error, positive and negative pulses but no proper signal */
	ehw_random
};

/** Structure containing all information of the current bit */
struct GB_result {
	/** I/O error while reading bit from hardware */
	bool bad_io;
	/** end of file data */
	bool done;
	/** the value of the currently received bit */
	enum eGB_bitvalue bitval;
	/** any (missing) minute marker, if applicable */
	enum eGB_marker marker;
	/** radio reception status */
	enum eGB_HW hwstat;
	/** skip state for reading log files */
	bool skip;
};

/**
 * Hardware parameters:
 */
struct hardware {
	/** sample frequency in Hz */
	unsigned freq;
	/** GPIO device number (FreeBSD only) */
	unsigned iodev;
	/** pin number to read from */
	unsigned pin;
	/** pin value is high (1) or low (0) for active signal */
	bool active_high;
};

/**
 * Detailed information about the radio reception:
 */
struct sGB_bitinfo {
	/**
	 * bit0 and bit20 were reset to their initial values (normally
	 * because of reception errors or fluctuations in CPU usage)
	 */
	bool bitlen_reset;
	/**
	 * The current value of the interval between two samples in microseconds.
	 */
	long interval;
	/**
	 * The interval between taking two samples (in microseconds) was changed.
	 */
	bool change_interval;
	/**
	 * The length of the active part of this bit in samples.
	 */
	int act;
	/**
	 * The raw received radio signal, 1 + {@link hardware.freq} / 4 items,
	 * with each item holding 8 bits
	 */
	unsigned char *signal;
	/**
	 * The portion of signal that is actually used.
	 */
	int cursor;
	/**
	 * the average length of the high part of bit 0 (a 0 bit) in samples
	 */
	float bit0;
	/**
	 * the average length of the high part of bit 20 (a 1 bit) in samples
	 */
	float bit20;
};
extern struct sGB_bitinfo bitinfo;

/**
 * Prepare for input from a log file.
 *
 * @param infilename The name of the log file to use.
 * @return Preparation was successful (0), -1 or errno otherwise.
 */
int set_mode_file(const char * infilename);

/**
 * Prepare for live input.
 *
 * The sample rate is set to {@link hardware.freq} Hz, reading from pin
 * {@link hardware.pin} using {@link hardware.active_high} logic.
 *
 * @param config The JSON object containing the parsed configuration from
 * config.json
 * @return Preparation was successful (0), -1 or errno otherwise.
 */
int set_mode_live(struct json_object *config);

/**
 * Return the hardware parameters parsed from {@link set_mode_live}.
 *
 * @return The hardware parameters.
 */
struct hardware get_hardware_parameters(void);

/**
 * Clean up when closing the device or input logfile, and closing the output
 *log file if applicable.
 */
void cleanup(void);

/**
 * Retrieve one pulse from the hardware.
 *
 * @return 0 or 1 depending on the pin value and {@link hardware.active_high},
 * or 2 if obtaining the pulse failed.
 */
int get_pulse(void);

/**
 * Retrieve one bit from the log file.
 *
 * @return The current bit from the log file and its associated state.
 */
struct GB_result get_bit_file(void);

/**
 * Prepare for the next bit: update the bit position or wrap it around.
 *
 * @param in_gbr The current bit state structure.
 *
 * @return The new bit state structure, with the marker field adjusted to
 * indicate state of the bit buffer and the minute ending.
 */
struct GB_result next_bit(struct GB_result in_gbr);

/**
 * Retrieve the current bit position.
 *
 * @return The current bit position (0..60).
 */
int get_bitpos(void);

/**
 * Determine if there should be a space between the last bit and the current
 * bit when displaying the bit buffer.
 *
 * @param bitpos The current bit position.
 * @return If a space is desired.
 */
bool is_space_bit(int bitpos);

/**
 * Open the log file and append a "new log" marker to it.
 *
 * @param logfilename The name of the log file to use.
 * @return The log file was opened successfully (0), or errno on error.
 */
int append_logfile(const char * logfilename);

/**
 * Close the currently opened log file.
 *
 * @return The log file was closed successfully (0), or errno otherwise.
 */
int close_logfile(void);

/**
 * Retrieve the accumulated minute length in milliseconds.
 *
 * @return The accumulated minute length in milliseconds.
 */
unsigned get_acc_minlen(void);

/**
 * Reset the accumulated minute length.
 */
void reset_acc_minlen(void);

/**
 * Retrieve the cutoff value written to the log file.
 *
 * @return The cutoff value.
 */
float get_cutoff(void);

/**
 * Flush the current log file to its storage location.
 *
 * @param arg Unused argument because of pthread_create()
 */
_Noreturn void *flush_logfile(void *arg);

/**
 * Write a character to the log file if it is open.
 *
 * @param chr The character to write.
 */
void write_to_logfile(char chr);

/**
 * Clear the cutoff value and the state values, except emark_toolong and
 * emark_late to be able to determine if this flag can be cleared again.
 *
 * @param in_gbr The current state.
 * @return The adjusted state.
 */
struct GB_result set_new_state(struct GB_result in_gbr);

#endif
