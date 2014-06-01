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

#ifndef DCF77PI_INPUT_H
#define DCF77PI_INPUT_H

/* this bit has value 1 */
#define GETBIT_ONE	(1 << 0)
/* end-of-minute bit */
#define GETBIT_EOM	(1 << 1)
/* end of data, either end-of-file or user quit */
#define GETBIT_EOD	(1 << 2)
/* bit value could not be determined */
#define GETBIT_READ	(1 << 3)
/* bit buffer would overflow */
#define GETBIT_TOOLONG	(1 << 4)
/* I/O error while reading bit from hardware */
#define GETBIT_IO	(1 << 5)
/* transmitter error, all positive pulses */
#define GETBIT_XMIT	(1 << 6)
/* receiver error, all negative pulses */
#define GETBIT_RECV	(1 << 7)
/* random radio error, both positive and negative pulses but no proper signal */
#define GETBIT_RND	(1 << 8)

#include <stdint.h> /* uintX_t */

/*
 * Hardware parameters:
 * freq        = sample frequency in Hz
 * pin         = pin number to read from
 * active_high = pin value is high (1) for active signal
 */
struct hardware {
	unsigned long freq;
	unsigned int pin, active_high;
};

/*
 * (Internal) information about the currently received bit:
 * tlow       = time in samples when the signal went low again
 * t          = length of this bit in samples
 * freq_reset = realfreq was reset to hw.freq (normally because of reception
 *    errors)
 * realfreq   = the average length of a bit in samples
 * bit0       = the average length of bit 0 (a 0 bit) in samples
 * bit20      = the average length of bit 20 (a 1 bit) in samples
 * frac       = tlow / t, or -1 % in case of an error
 * maxone     = the maximum allowed length of the high part of the pulse for
 *   a 1 bit, or -1 % in case of an error
 * a          = the amount to update the wave of the pulse with
 *   (see blinkenlight link in README.md)
 */
struct bitinfo {
	int tlow, t, freq_reset;
	float realfreq, bit0, bit20; /* static */
	float frac, maxone, a;
};

/**
 * Prepare for input from a log file.
 *
 * @param infilename The name of the log file to use.
 * @return Preparation was succesful (0), errno otherwise.
 */
int set_mode_file(char *infilename);

/**
 * Prepare for live input.
 *
 * The sample rate is set to hw.freq Hz, reading from pin hw.pin using
 * hw.active_high logic.
 *
 * @return Preparation was succesful (0), -1 or -errno otherwise.
 */
int set_mode_live(void);

/**
 * Return the hardware paramters.
 *
 * @return The hardware parameters.
 */
struct hardware *get_hardware_parameters(void);

/**
 * Clean up when: close the device or input logfile, and output log file
 *   if applicable.
 */
void cleanup(void);

/**
 * Retrieve one pulse from the hardware.
 *
 * @return: 0 or 1 depending on the pin value and hw.active_high, or
 *   GETBIT_IO if obtaining the pulse failed.
 */
uint8_t get_pulse(void);

/**
 * Retrieve one bit from the log file.
 *
 * @return The current bit from the log file, a mask of GETBIT_* values.
 */
uint16_t get_bit_file(void);

/**
 * Retrieve one live bit from the hardware.
 * This function determines several values which can be retrieved using
 * get_bit_info().
 *
 * @return The currently received bit, a mask of GETBIT_* values.
 */
uint16_t get_bit_live(void);

/**
 * Prepare for the next bit: update the bit position or wrap it around.
 *
 * @return The current bit state mask, ORed with GETBIT_TOOLONG if the
 *   bit buffer just became full.
 */
uint16_t next_bit(void);

/**
 * Retrieve the current bit position.
 *
 * @return The current bit position (0..60).
 */
uint8_t get_bitpos(void);

/**
 * Retrieve the current bit buffer.
 *
 * @return The bit buffer, an array of 0 and 1 values.
 */
uint8_t *get_buffer(void);

/**
 * Determine if there should be a space between the last bit and the current
 * bit when displaying the bit buffer.
 *
 * @param bit The current bit position.
 */
int is_space_bit(int bitpos);

/**
 * Open the log file and append a "new log" marker to it.
 *
 * @param logfile The name of the log file to use.
 * @return The log file was opened succesfully (0), or errno on error.
 */
int write_new_logfile(char *logfile);

/**
 * Close the currently opened log file.
 *
 * @return The log file was closed successfully (0), or errno otherwise.
 */
int close_logfile(void);

/**
 * Retrieve "internal" information about the currently received bit.
 *
 * @return The bit information as described for {@link struct bitinfo}.
 */
struct bitinfo *get_bitinfo(void);

#endif
