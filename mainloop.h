/*
Copyright (c) 2014, 2016 René Ladan. All rights reserved.

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

#ifndef DCF77PI_MAINLOOP_H
#define DCF77PI_MAINLOOP_H

#include <stdbool.h>
struct DT_result;
struct GB_result;
struct alm;
struct tm;

/** User input which controls the client */
struct ML_result {
	bool change_logfile;
	bool quit;
	bool settime;
	char *logfilename;
};

/**
 * Provide a ready-to-use mainloop function for the main program. Both dcf77pi
 * and dcf77pi-analyze use it.
 *
 * @param logfilename The name of the log file to write the live data to or
 *   NULL if not in live mode.
 * @param get_bit The callback to obtain a bit (either live or from a log file).
 * @param display_bit The callback to display the currently received bit
 *   (either live or from a log file).
 * @param display_long_minute The callback to indicate that this minute is too
 *   long (eGB_too_long is set).
 * @param display_minute The callback to display information about the current
 *   minute.
 * @param display_new_second The optional callback for additional actions after
 *   the bit is displayed and the minute information is updated.
 * @param display_alarm The callback to display third party alarm messsages.
 * @param display_unknown The callback to display unknown third party messages.
 * @param display_weather The callback to display third party weather messages.
 * @param display_time The callback to display the decoded time.
 * @param display_thirdparty_buffer The callback to display the third party
 *   buffer.
 * @param show_mainloop_result The optional callback to display the result of
 *   actions performed by {@link mainloop}.
 * @param process_input The optional callback to handle interactive user input.
 * @param post_process_input The optional callback to finish handling
 *   interactive user input.
 */
void mainloop(char *logfilename,
    struct GB_result (*get_bit)(void),
    void (*display_bit)(struct GB_result, int),
    void (*display_long_minute)(void),
    void (*display_minute)(int),
    void (*display_new_second)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(struct DT_result, struct tm),
    void (*display_thirdparty_buffer)(const unsigned[]),
    struct ML_result (*show_mainloop_result)(struct ML_result, int),
    struct ML_result (*process_input)(struct ML_result, int),
    struct ML_result (*post_process_input)(struct ML_result, int));

/**
 * Get the result value set by {@link mainloop}.
 *
 * @return The result value set by {@link mainloop}.
 */
int get_mainloop_result(void);

#endif
