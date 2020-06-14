// Copyright 2014-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DCF77PI_MAINLOOP_LIVE_H
#define DCF77PI_MAINLOOP_LIVE_H

#include "setclock.h"

#include <stdbool.h>

struct DT_result;
struct GB_result;
struct alm;
struct tm;

/** User input which controls the client */
struct ML_result {
	/** Request to change the name of the log file */
	bool change_logfile;
	/** Request to quit the program */
	bool quit;
	/** Request to set the system time upon each valid minute */
	bool settime;
	/** Result of setting the system time */
	enum eSC_status settime_result;
	/** The name of the log file */
	char *logfilename;
};

/**
 * Provide a ready-to-use mainloop function for the live program.
 *
 * @param logfilename The name of the log file to write the live data to.
 * @param display_bit The callback to display the currently received bit.
 * @param display_long_minute The callback to indicate that this minute is too
 * long (eGB_too_long is set).
 * @param display_minute The callback to display information about the current
 * minute.
 * @param display_new_second The callback for additional actions
 * after the bit is displayed and the minute information is updated.
 * @param display_alarm The callback to display third party alarm messsages.
 * @param display_unknown The callback to display unknown third party
 * messages.
 * @param display_weather The callback to display third party weather
 * messages.
 * @param display_time The callback to display the decoded time.
 * @param display_thirdparty_buffer The callback to display the third party
 * buffer.
 * @param process_setclock_result The callback to display the result
 * of setting the system clock.
 * @param process_input The callback to handle interactive user
 * input.
 * @param post_process_input The callback to finish handling
 * interactive user input.
 */
void mainloop_live(
    char *logfilename,
    void (*display_bit)(struct GB_result, int),
    void (*display_long_minute)(void),
    void (*display_minute)(int),
    void (*display_new_second)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(struct DT_result, struct tm),
    void (*display_thirdparty_buffer)(const unsigned[]),
    struct ML_result (*process_setclock_result)(struct ML_result, int),
    struct ML_result (*process_input)(struct ML_result, int),
    struct ML_result (*post_process_input)(struct ML_result, int));

#endif
