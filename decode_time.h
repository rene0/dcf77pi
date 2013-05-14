#ifndef DCF77PI_DECODE_TIME_H
#define DCF77PI_DECODE_TIME_H

#include <stdint.h>
#include <time.h>

#define DT_DSTERR	1
#define DT_MIN		2
#define DT_HOUR		4
#define DT_DATE		8
#define DT_B0		16
#define DT_B20		32
#define DT_XMIT		64
#define DT_SHORT	128
#define DT_LONG		256
#define DT_CHDST	512
#define DT_LEAP		1024

#define ANN_CHDST	1
#define ANN_LEAP	2

int add_minute(struct tm *time, int flags);
int decode_time(int init2, int minlen, uint8_t *buffer, struct tm *time);
void display_time(int init2, int dt, struct tm oldtime, struct tm time);

#endif
