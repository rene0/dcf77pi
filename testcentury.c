/*
Copyright (c) 2016 René Ladan. All rights reserved.

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
