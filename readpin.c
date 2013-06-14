#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "input.h"

int
main(int argc, char **argv)
{
	unsigned long high, low, err;
	struct gpio_req req;
	int fd;
	int old;
	int res;
	struct hardware hw;

	res = read_hardware_parameters("hardware.txt", &hw);
	if (res)
		return res;

	fd = init_hardware(hw.pin);
	if (fd < 0)
		return fd; /* errno */

	high = low = err = 0;
	old = 0;
	for (;;) {
		req.gp_pin = hw.pin;
		if (ioctl(fd, GPIOGET, &req) < 0) {
			perror("ioctl(GPIOGET)"); /* whoops */
			cleanup();
			return 1;
		}
		if ((req.gp_value != old && old == 0) ||
		    (high + low > hw.freq)) {
			printf("%lu %lu %lu\n", err, high, low);
			high = low = 0;
		}
		if (req.gp_value == GPIO_PIN_HIGH)
			high++;
		else if (req.gp_value == GPIO_PIN_LOW)
			low++;
		else
			err++; /* Houston? */
		old = req.gp_value;
		(void)usleep(1000000.0 / hw.freq); /* us */
	}
	if (close(fd))
		perror("close");
	return 0;
}
