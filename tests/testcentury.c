// Copyright 2016 René Ladan
// SPDX-License-Identifier: BSD-2-Clause

#include "calendar.h"

#include <stdio.h>
#include <time.h>

int
main(int argc, char *argv[])
{
	 /* start with 1900-01-01 = Monday (matches `ncal 1900`) */
	struct tm time;
	time.tm_wday = 1; /* Monday */
	/* check for every date if it matches */
	for (int century = 0; century <  4; century++)
		for (time.tm_year = 0; time.tm_year < 100; time.tm_year++)
			for (time.tm_mon = 1; time.tm_mon < 13; time.tm_mon++) {
				int lday;

				time.tm_year += 1900 + century * 100;
				lday = lastday(time);
				time.tm_year -= 1900 + century * 100;
				for (time.tm_mday = 1; time.tm_mday <= lday;
				    time.tm_mday++) {
					int co;

					co = century_offset(time);
					if (co != century)
						printf("%d-%d-%d,%d : %d "
						    "should be %d\n",
						    time.tm_year, time.tm_mon,
						    time.tm_mday, time.tm_wday,
						    co, century);
					if (++time.tm_wday == 8)
						time.tm_wday = 1;
				}
			}
	printf("done\n");
	return 0;
}
