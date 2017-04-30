Version 3.6.0 --

Version 3.5.0 -- 2016-

Version 3.4.2 --

Version 3.4.1 --

Version 3.4.0 --

Version 3.3.0 --

Version 3.2.0 --

Version 3.1.0 --

Version 3.0.0.1 --

Version 3.0.0 --
================

* merge lib branch

Version 2.1.0 -- 2014-05-11
* readpin: add a -q (quiet) parameter to suppress displaying of the raw signal
* determine maxzero and maxone dynamically using the currently received lengths
  of bit 0 respectively bit 20, if received OK
* readpin: various displaying updates, including for bit 0 and 20
* add a state to indicate that a bit has been received but that its length (in
  samples) is not fully settled yet
* show bit0 and bit20, the radio state and maxone in the GUI
* remove maxzero and maxone from config.txt, they are now determined dynamically
* update README.md
* always refresh the bit data in GUI mode not only on a new second to be able
  to show it in case of reception errors
* fix displaying of German civil warning in file mode
* do not set the DST jump flag if bit 17 and 18 are equal

Version 2.0.3 -- 2014-04-13
* relax DST processing in case of reception errors
* always reset DST change and leap second announcement flags at hh:00 to prevent
  them from being set forever in case of reception errors
* detect unexpected jumps to non-DST next to unexpected jumps to DST
* mention the backspace key in README.md

Version 2.0.2 -- 2014-03-31
* code restructuring, move GUI functionality to new file guifuncs.[ch] and
  code to set the system clock to setclock.[ch]
* add March and September as valid months for leap seconds, per ITU-R TF.460-6
* replace the -l parameter by a new variable 'outlogfile' in config.txt
* forbid values without keys in config.txt
* reset the frequency reset light once the frequency is OK again
* fix displaying of the German civil warning in GUI mode
* fix a bug which would leave the pulse counter at -1
* introduce the 'L' key to change the name of the log file (including backspace
  and scrolling functionality), use '.' to keep the current log file
* plug some memory leaks
* fix a buffer overflow bug when reading the key values of config.txt
* update README.md for new GUI functionality and 'outlogfile' variable
* be more flexible with end-of-line markers in the input log file
* show bit read errors in the GUI
* ensure that the hour value is considered valid on the DST transition itself,
  bug introduced in version 1.1.1

Version 2.0.1 -- 2014-02-21
* Makefile: add various targets, use standard DESTDIR instead of inventing our
  own
* only display the third-party buffer each three minutes if it is fully received
* plug ncurses memory leak

Version 2.0.0 -- 2014-02-16 - merge gui branch
==============================================
* ncurses is used for the GUI (so technically it is a text-user interface)
* GUI shows both the previous and currently received minute, with full decoding
* available keys are shown at the bottom of the screen, messages on the line
  above it
* parameter -v removed, implicit in the GUI
* parameter -S removed, replaced by a toggleable 'S' key in the GUI
* exit live mode with 'Q'
* add GUI version of all error conditions, bit errors are shown as a yellow
  version of the previous value instead of as an underscore
* show wall clock minute length
* show German civil warnings
* show the contents of the third party buffer in GUI mode

Version 1.1.3 -- 2014-01-26
* fix build with GCC 4.5
* add NetBSD as a supported platform, but without live mode
* check leapsecmonth values in config.txt, only add them if they are legal
* check summermonth and wintermonth values, set to 0 if invalid (this means
  either DST set respectively reset is never valid)
* allow the predicted time to advance by more than one minute, useful in case
  of thunderstorms

Version 1.1.2 -- 2013-12-26
* make the year field 4 digits wide
* bound realfreq in case of thunderstorms
* copy new realfreq code from readpin to the main program, remove realfreq from
  config.txt

Version 1.1.1 -- 2013-12-15
* calculate the century from the given year, month, day-of-month and
  day-of-week, set an error upon failure
* ignore time offset changes if bit 17 and 18 are equal
* set an error if the day-of-month value is too large
* readpin: use parameter -t instead of conditional compilation for time tuning
* fix an edge case where DST would never be valid
* allow setting the system time using the -S parameter, idea from "Guenter"
* plug some file descriptor leaks
* improve resilience against reception errors
* readpin: determine 'realfreq' variable dynamically

Version 1.1.0 -- 2013-12-02
* Linux fixes
* weaken conditions for processing leap seconds and DST changes (setting those
  is not affected)
* increase resilience against noise to prevent false '1' bits
* some fixes from splint
* replace bit detection algorithm by an idea from Udo Klein, with permission.
  The new algorithm improves detection reduces bit detection errors by 99%
* update variables in config.txt
* update README.md for new algorithm and variables
* improve verbose output for readability
* readpin: Conditionally compile in nanosleep() time tuning (i.e. adjusting for
  the time it takes to obtain a pulse)

Version 1.0.5 -- 2013-10-28
* update for stage support in FreeBSD
* compile with debug information
* add a "lint" target to check the code for both FreeBSD and Linux
* some "make lint" fixes
* readpin: subtract the time it takes to get a pulse from the sleep time to
  bring the number of actual samples closer to the specified frequency, show
  the per-pulse sleep difference at the end of each second
* readpin: do not hardcode the hardware filename
* display different symbols for missing end-of-minute marker and too many bits
* decode if the end-of-minute marker is missing
* show the bit position and the state when the -v parameter is given
* show when the DST or leap second flags are erroneously set (not on the last
  Sunday or last day of a month), do not process them in these cases
* only adjust the hour for DST changes on the last Sunday of the month, 00:59Z
* synchronize radio code with that of readpin
* replace hardware.txt by config.txt, which uses key/value pairs instead of
  only values, which makes it much more flexible (no static order, check for
  missing values) and future-proof. It now also contains the values for months
  in which leap seconds and DST changes can occur
* prevent spurious "Year value jump" messages
* fix a harmless buffer overflow

Version 1.0.4 -- 2013-09-10
* readpin: add a bit counter and explicitly show new minutes and pulse values
* improve radio reception and end-of-minute detection, for example during
  thunderstorms
* detect the tail of the end-of-minute marker at startup
* do not insist on a minimum wall clock minute length when an end-of-minute
  marker is received, instead add the elapsed time to the expected time
* check received time values per digit
* only automatically increment the time if the wall clock minute length is long
  enough to prevent the received time from increasing too fast
* only announce DST changes or leap seconds if the received date and time are
  valid
* prevent false "date/time jumped" messages if the minute is too long
* differentiate between DST errors (bit 17 and bit 18 are equal) and sudden DST
  changes, the latter are ignored
* check that there is no error when changing DST on the hour when announced
* check that there is no error when processing the leap second when announced
* use a bogus minute length if the end-of-minute marker was missed or bits are
  split up

Version 1.0.3.1 -- 2013-07-22
* move hardware.txt to an etc/dcf77pi subdirectory for package installation

Version 1.0.3 -- 2013-07-21
* do not rely on the system clock to determine if the current year is a leap
  year
* keep track of the wall clock minute length and only decode the date/time
  when it is long enough to avoid the clock from wandering off if no useful
  data is received
* display the wall clock minute length at the end of each minute
* add a timeout for transmitter errors (all pulses 1) as 2.5 seconds
* add a separate state for "random" errors (reception timeout with both 0 and 1
  pulses)
* synchronize end-of-minute handling between live and log file reception
* allow reading log files from DOS and Mac
* only support live reception on FreeBSD if it is new enough (9.0-RELEASE)
* install hardware.txt into a valid location

Version 1.0.2 -- 2013-06-24
* readpin: display pulses as characters, show the active portion of the bit
* fix handling of -f and -l parameters
* detect if all pulses are 0 ("receiver error")
* detect reading an unknown character from the log file
* check that leap second always have value 0
* improve robustness of radio reception
* explicitly log the end-of-minute marker
* use automatically incremented time when decoded values are incorrect
* improve error messages
* allow DST changes when all previously received minutes contained an error
* indicate receiver and transmitter errors when decoding the log file

Version 1.0.1 -- 2013-06-22
* add an "install" target to Makefile

Version 1.0.0 -- 2013-06-21
* initial release

Initial commit -- 2013-05-14
