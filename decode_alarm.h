#ifndef DCF77PI_DECODE_ALARM_H
#define DCF77PI_DECODE_ALARM_H

#include <stdint.h>

/*
 * From German wikipedia mostly, long regions and parities are unspecified
 * short regions: 1=nord, 2=middle, 4=south
 */
void display_alarm(uint8_t *buf);

#endif
