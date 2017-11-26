/*
Copyright (c) 2013-2017 René Ladan. All rights reserved.

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

#ifndef DCF77PI_INPUT_H
#define DCF77PI_INPUT_H

#include <stdbool.h>

struct json_object;

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

/** Structure containing all information of the currnet bit */
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
struct bitinfo {
	/** bit0 and bit20 were reset to their initial values (normally because
	  * of reception errors or fluctuations in CPU usage) */
	bool bitlen_reset;
	/** realfreq was reset to {@link hardware.freq} (normally because of
	  * reception errors or fluctuations in CPU usage) */
	bool freq_reset;
	/** time in samples when the signal went low again, -1 initially */
	int tlow;
	/** time in samples when the signal was last measured as 0,
	  * -1 initially */
	int tlast0;
	/** length of this bit in samples */
	unsigned t;
	/** the raw received radio signal, {@link hardware.freq} / 2 items,
	  * with each item holding 8 bits */
	unsigned char *signal;
	/** the average length of a bit in samples */
	unsigned long long realfreq;
	/** the average length of the high part of bit 0 (a 0 bit) in
	  * samples */
	unsigned long long bit0;
	/** the average length of the high part of bit 20 (a 1 bit) in
	  * samples */
	unsigned long long bit20;
};

/**
 * Prepare for input from a log file.
 *
 * @param infilename The name of the log file to use.
 * @return Preparation was succesful (0), -1 or errno otherwise.
 */
int set_mode_file(const char * const infilename);

/**
 * Prepare for live input.
 *
 * The sample rate is set to {@link hardware.freq} Hz, reading from pin
 * {@link hardware.pin} using {@link hardware.active_high} logic.
 *
 * @param config The JSON object containing the parsed configuration from
 *  config.json
 * @return Preparation was succesful (0), -1 or errno otherwise.
 */
int set_mode_live(struct json_object *config);

/**
 * Return the hardware parameters.
 *
 * @return The hardware parameters.
 */
struct hardware get_hardware_parameters(void);

/**
 * Clean up when: close the device or input logfile, and output log file if
 * applicable.
 */
void cleanup(void);

/**
 * Retrieve one pulse from the hardware.
 *
 * @return 0 or 1 depending on the pin value and {@link hardware.active_high},
 *   or 2 if obtaining the pulse failed.
 */
int get_pulse(void);

/**
 * Retrieve one bit from the log file.
 *
 * @return The current bit from the log file and its associated state.
 */
struct GB_result get_bit_file(void);

/**
 * Retrieve one live bit from the hardware. This function determines several
 * values which can be retrieved using get_bitinfo().
 *
 * @return The currently received bit and its full status.
 */
struct GB_result get_bit_live(void);

/**
 * Prepare for the next bit: update the bit position or wrap it around.
 *
 * @return The current bit state structure, with the marker field adjusted
 *   to indicate state of the bit buffer and the minute end.
 */
struct GB_result next_bit(void);

/**
 * Retrieve the current bit position.
 *
 * @return The current bit position (0..60).
 */
int get_bitpos(void);

/**
 * Retrieve the current bit buffer.
 *
 * @return The bit buffer, an array of 0 and 1 values.
 */
const int * const get_buffer(void);

/**
 * Determine if there should be a space between the last bit and the current
 * bit when displaying the bit buffer.
 *
 * @param bitpos The current bit position.
 */
bool is_space_bit(int bitpos);

/**
 * Open the log file and append a "new log" marker to it.
 *
 * @param logfilename The name of the log file to use.
 * @return The log file was opened succesfully (0), or errno on error.
 */
int append_logfile(const char * const logfilename);

/**
 * Close the currently opened log file.
 *
 * @return The log file was closed successfully (0), or errno otherwise.
 */
int close_logfile(void);

/**
 * Retrieve "internal" information about the currently received bit.
 *
 * @return The bit information as described for {@link bitinfo}.
 */
struct bitinfo get_bitinfo(void);

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
 * @return The cutoff value (multiplied by 10,000)
 */
int get_cutoff(void);

#endif
