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

#include "guifuncs.h"

int
init_curses(void)
{
	initscr();
	if (has_colors() == FALSE)
		return 1;
	start_color();
	init_pair(1, COLOR_RED, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK); /* turn on A_BOLD */
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(7, COLOR_WHITE, COLOR_BLACK);
	init_pair(8, COLOR_BLACK, COLOR_BLACK); /* A_INVIS does not work? */

	noecho();
	nonl();
	cbreak();
	nodelay(stdscr, TRUE);
	curs_set(0);
	refresh(); /* prevent clearing windows upon getch() / refresh() */

	return 0;
}

void
statusbar(WINDOW *win, char *fmt, ...)
{
	va_list ap;

	wmove(win, 0, 0);
	va_start(ap, fmt);
	vwprintw(win, fmt, ap);
	va_end(ap);
	wclrtoeol(win);
	wrefresh(win);
}

void
draw_keys(WINDOW *win)
{
	mvwprintw(win, 0, 0, "[Q] -> quit [S] -> toggle time sync");
	mvwchgat(win, 0, 1, 1, A_BOLD, 4, NULL); /* [Q] */
	mvwchgat(win, 0, 13, 1, A_BOLD, 4, NULL); /* [S] */
	wrefresh(win);
}

void
check_timer(WINDOW *win, int *old_bitpos, int bitpos)
{
	if (*old_bitpos != -1 && (bitpos % 60 == (*old_bitpos + 2) % 60 ||
	    (*old_bitpos == 57 && bitpos == 0))) {
		/*
		 * Time for status text passed, cannot use *sleep()
		 * in statusbar() because that pauses reception
		 */
		*old_bitpos = -1;
		draw_keys(win);
	}
}
