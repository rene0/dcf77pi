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

#ifndef DCF77PI_DECODE_TIME_H
#define DCF77PI_DECODE_TIME_H

#include <stdint.h>
#include <time.h>

/* update every 400 years, now at 2400-01-01 */
#define BASEYEAR	2000

#define DT_DSTERR	(1 << 0)
#define DT_MIN		(1 << 1)
#define DT_HOUR		(1 << 2)
#define DT_DATE		(1 << 3)
#define DT_B0		(1 << 4)
#define DT_B20		(1 << 5)
#define DT_SHORT	(1 << 6)
#define DT_LONG		(1 << 7)
#define DT_DSTJUMP	(1 << 8)
#define DT_CHDSTERR	(1 << 9)
#define DT_LEAPERR	(1 << 10)
#define DT_MINJUMP	(1 << 11)
#define DT_HOURJUMP	(1 << 12)
#define DT_MDAYJUMP	(1 << 13)
#define DT_WDAYJUMP	(1 << 14)
#define DT_MONTHJUMP	(1 << 15)
#define DT_YEARJUMP	(1 << 16)
#define DT_LEAPONE	(1 << 17)
/* leap second should always be zero if present */
#define DT_XMIT		(1 << 18)
#define DT_CHDST	(1 << 19)
#define DT_LEAP		(1 << 20)

#define ANN_CHDST	(1 << 30)
#define ANN_LEAP	(1 << 31)

void init_time(void); /* initialize month values from configuration */
void add_minute(struct tm *time);
uint32_t decode_time(int init, int minlen, uint8_t *buffer,
    struct tm *time, int *acc_minlen);
int get_utchour(struct tm time);
char *get_weekday(int wday);

#endif
