// Copyright 2014-2020 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#ifndef DCF77PI_MAINLOOP_ANALYZE_H
#define DCF77PI_MAINLOOP_ANALYZE_H

struct DT_result;
struct GB_result;
struct alm;
struct tm;

/**
 * Provide a ready-to-use mainloop function for the analyzer program.
 *
 * @param display_bit The callback to display the currently received bit.
 * @param display_long_minute The callback to indicate that this minute is too
 * long (eGB_too_long is set).
 * @param display_minute The callback to display information about the current
 * minute.
 * @param display_alarm The callback to display third party alarm messages.
 * @param display_unknown The callback to display unknown third party
 * messages.
 * @param display_weather The callback to display third party weather
 * messages.
 * @param display_time The callback to display the decoded time.
 * @param display_thirdparty_buffer The callback to display the third party
 * buffer.
 */
void mainloop_analyze(
    void (*display_bit)(struct GB_result, int),
    void (*display_long_minute)(void),
    void (*display_minute)(int),
    void (*display_alarm)(struct alm),
    void (*display_unknown)(void),
    void (*display_weather)(void),
    void (*display_time)(struct DT_result, struct tm),
    void (*display_thirdparty_buffer)(const unsigned[]));

#endif
