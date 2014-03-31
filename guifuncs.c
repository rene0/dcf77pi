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

#include <stdint.h>
#include <string.h>

int old_bitpos = -1; /* timer for statusbar inactive */
int input_mode = 0;  /* normal input (statusbar keys) or string input */
char keybuf[MAXBUF]; /* accumulator for string input */
uint8_t input_count, input_xpos;
int msglen;

int
init_curses(void)
{
	initscr();
	if (has_colors() == FALSE)
		return -1;
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
	keypad(stdscr, TRUE);
	curs_set(0);
	refresh(); /* prevent clearing windows upon getch() / refresh() */

	return 0;
}

void
statusbar(WINDOW *win, int bitpos, char *fmt, ...)
{
	va_list ap;

	old_bitpos = bitpos;

	wmove(win, 1, 0);
	va_start(ap, fmt);
	vw_printw(win, fmt, ap);
	va_end(ap);
	wclrtoeol(win);
	wrefresh(win);
}

void
draw_keys(WINDOW *win)
{
	mvwprintw(win, 1, 0, "[Q] -> quit [L] -> change log file"
	    " [S] -> toggle time sync");
	wclrtoeol(win);
	mvwchgat(win, 1, 1, 1, A_BOLD, 4, NULL); /* [Q] */
	mvwchgat(win, 1, 13, 1, A_BOLD, 4, NULL); /* [L] */
	mvwchgat(win, 1, 36, 1, A_BOLD, 4, NULL); /* [S] */
	wrefresh(win);
}

void
check_timer(WINDOW *win, int bitpos)
{
	if (old_bitpos != -1 && (bitpos % 60 == (old_bitpos + 2) % 60 ||
	    (old_bitpos == 57 && bitpos == 0))) {
		/*
		 * Time for status text passed, cannot use *sleep()
		 * in statusbar() because that pauses reception
		 */
		old_bitpos = -1;
		draw_keys(win);
	}
}

void
input_line(WINDOW *win, char *msg)
{
	mvwprintw(win, 1, 0, "%s", msg);
	wclrtoeol(win);
	wrefresh(win);
	input_mode = 1;
	input_count = 0;
	input_xpos = strlen(msg);
	msglen = strlen(msg);
}

void
end_input(WINDOW *win)
{
	keybuf[input_count] = '\0'; /* terminate to prevent overflow */
	draw_keys(win);
	input_mode = -1;
}

void
process_key(WINDOW *win, int inkey)
{
	char dispbuf[80];

	if (inkey == KEY_BACKSPACE || inkey == '\b' || inkey == 127) {
		if (input_count > 0) {
			input_count--;
			input_xpos--;
			if (input_xpos > 78) {
				/* Shift display line one character to right */
				(void)strncpy(dispbuf, keybuf +
				    (input_xpos - 79), 79 - msglen);
				dispbuf[80 - msglen] = '\0';
				mvwprintw(win, 1, msglen + 1, "%s", dispbuf);
			} else {
				wmove(win, 1, input_xpos + 1);
				wclrtoeol(win);
			}
			wrefresh(win);
		}
	} else if ((inkey == KEY_ENTER || inkey == '\r' || inkey == '\n') ||
	    input_count == MAXBUF-1)
		end_input(win);
	else {
		keybuf[input_count++] = inkey;
		input_xpos++;
		if (input_xpos > 79) {
			/* Shift displayed line one character to the left */
			(void)strncpy(dispbuf, keybuf +
			    (input_xpos - 79), 79 - msglen);
			dispbuf[80 - msglen] = '\0';
			mvwprintw(win, 1, msglen + 1, "%s", dispbuf);
		} else
			mvwprintw(win, 1, input_xpos, "%c", inkey);
		wrefresh(win);
	}
}

int
get_inputmode(void)
{
	return input_mode;
}

void
set_inputmode(int mode)
{
	input_mode = mode;
}

char *
get_keybuf(void)
{
	return keybuf;
}
