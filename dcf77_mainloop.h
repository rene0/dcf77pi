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

#include <time.h>

int dcf77_mainloop(struct bitinfo *bi, char *logfilename, uint16_t (*get_bit)(void), void (*display_bit)(uint16_t, int), void (*print_long_minute)(void), void (*print_minute)(int, int), void (*process_new_minute)(void), void (*display_alarm)(struct alm), void (*display_alarm_error)(), void (*display_alarm_ok)(), void (*display_time)(uint32_t, struct tm), void (*print_civil_buffer)(uint8_t *), void (*set_time)(int, uint32_t, uint16_t, int, struct tm), void (*process_input)(uint16_t *, int, char *, int *, int *), void (*post_process_input)(char **, int *, uint16_t *, int));

#endif
