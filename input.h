#ifndef DCF77PI_INPUT_H
#define DCF77PI_INPUT_H

#define GETBIT_ONE	1
#define GETBIT_EOM	2
#define GETBIT_EOD	4
#define GETBIT_READ	8
#define GETBIT_TOOLONG	16
#define GETBIT_IO	32

int set_mode(int live, char *filename);
void cleanup(void);
int get_bit(void); /* stores result in internal buffer */
void display_bit(void);
int next_bit(void); /* prepare for next bit */
int get_bitpos(void);
uint8_t *get_buffer(void);

#endif
