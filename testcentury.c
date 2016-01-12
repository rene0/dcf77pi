#include "decode_time.h"
#include <stdio.h>

int
main(int argc, char *argv[])
{
	 /* start with 2000-01-01 = Saturday (matches `ncal 2000`) */
	uint8_t century, lday, co;
	struct tm time;
	time.tm_wday = 6; /* Saturday */
	/* check for every date if it matches */
	for (century = 0; century <  4; century++)
		for (time.tm_year = 0; time.tm_year < 100; time.tm_year++)
			for (time.tm_mon = 1; time.tm_mon < 13; time.tm_mon++) {
				time.tm_year += 2000 + century * 100;
				lday = lastday(time);
				time.tm_year -= 2000 + century * 100;
				for (time.tm_mday = 1; time.tm_mday <= lday; time.tm_mday++) {
					co = century_offset(time);
					if (co != century)
						printf("%d-%d-%d,%d : %d should be %d\n", time.tm_year, time.tm_mon, time.tm_mday, time.tm_wday, co, century);
					time.tm_wday++;
					if (time.tm_wday == 8)
						time.tm_wday = 1;
				}
			}
	printf("done\n");
	return 0;
}
