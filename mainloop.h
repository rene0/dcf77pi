/*
Copyright (c) 2014 René Ladan. All rights reserved.

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

#include "decode_alarm.h"
#include "input.h"

#include <stdint.h>
#include <time.h>

/**
 * Provide a ready-to-use mainloop function for the main program. Both dcf77pi
 * and dcf77pi-analyze use it.
 *
 * @param logfilename The name of the log file to write the live data to or NULL
 *   if not in live mode.
 * @param get_bit The callback to obtain a bit (either live or from a log file).
 * @param display_bit The callback to display the currently received bit (either
 *   live or from a log file).
 * @param display_long_minute The callback to indicate that this minute is too
 *   long (GETBIT_TOOLONG is set).
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
 * @param set_time The optional callback to set the system time after a valid
 *   minute is received.
 * @param process_input The optional callback to handle interactive user input.
 * @param post_process_input The optional callback to finish handling
 *   interactive user input.
 * @return Any error that happened (currently just 0).
 */
int mainloop(char *logfilename,
    uint16_t (*get_bit)(void),
    void (*display_bit)(uint16_t, int),
    void (*display_long_minute)(void),
    void (*display_minute)(unsigned int),
    void (*display_new_second)(void),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(uint32_t, struct tm),
    void (*display_thirdparty_buffer)(uint8_t *),
    void (*set_time)(uint8_t, uint32_t, uint16_t, int, struct tm),
    void (*process_input)(uint16_t *, int, char *, int *, int *),
    void (*post_process_input)(char **, int *, uint16_t *, int));

#endif
