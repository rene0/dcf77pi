Version 3.7.0 --
* Replace verbose license texts in source files by SPDX identifiers.
* Makefile: always install LICENSE.md and fix uninstall target.
* Replace config.txt by config.json, its equivalent in JSON. Update Makefile
  to compile/link various source files against json-c. set\_mode\_live() now
  takes a json\_object\* parameter that contains the parsed configuration.
  Remove now obsolete config.c and config.h [closes issue #11]
* Fix revival of issue #19.
* Do not use leapsecmonth to determine when a leap second announcement is
  valid, but count the number of valid 1 values of bit 19 in the preceding
  hour. If at least 50% of them are valid, the announcement is considered
  valid [closes issue #24 #31].
* Anologous to the leap second announcement, use the number of valid 1 values
  of bit 16 instead of summermonth and wintermonth. [closes issue #24 #31]
* Make fields dst\_announce and leap\_announce of DT\_result a boolean as
  invalid announcements are now impossible [closes issue #24].
* Do not insist on DST changes only being valid on the last Sunday of the
  month at 01:00 UTC in add\_minute() and substract\_minute() [closes issue #24]
* Remove leapsecmonth, summermonth and wintermonth from config.txt [closes
  issue #24]
* replace get\_utchour() by get\_utctime() which takes date changes into
  account [closes issue #28]
* dcf77pi: key 'u' toggles between UTC and local time [closes issue #28]
* Makefile: link testcentury only against calendar.o instead of libdcf77.so
* Rename testcentury to test\_calendar and add tests for all public functions
  in calendar.h
* Move unit tests into their own subdirectory "tests"
* Add a unit test for bits1to14.c [issue #3]
* dcf77pi-analyze: no need to depend on the configuration file after removal
  of the month parameters.
* Only allow to set the system clock on the minute.
* Use new enumeration eSC\_status to report the result of setting the system
  clock instead of an integer, return this value via a new field in
  ML\_result instead of using get\_mainloop\_result()
* Rename show\_mainloop\_result() into process\_setclock\_result()
* Return move values from sysexists.h in set\_mode\_live()
* dcf77pi: increase the time of status bar messages to four seconds.
* Update README for #11, #24 and #30.3
* Return the modified time of add\_minute() and substract\_minute() as a
  value instead of as a pointer.
* Convert get\_utchour() into get\_utctime(), take date into account and
  only covert to UTC when the time offset is valid [issue #28]
* dcf77pi: show the status of the 'S' button in the status bar
* Makefile: remove "test" target
* Improve reception algorithm in get\_bit\_live(), be more strict on updating
  realfreq and the lengths of bit 0 and bit 20.
* Flush the log file every 60 seconds [issue #10]
* Add an SVG version of the receiver schematics
* Fix initializing the signal GPIO pin under FreeBSD, this happened to work
  by chance on the Raspberry Pi
* Check against NULL filenames in input.c
* Redraw the static parts of the screen when the window size changes
  [issue #30.2]
* Doxygen fixes

Version 3.6.1 -- 2017-11-19
* Makefile: drop splint target, does not work with Clang 4.0.0/FreeBSD 12
  nor with GCC 5.4.0/Ubuntu 16.04 [issue #6]
* Makefile: drop lint target, this is severely outdated [issue #6]
* make code compliant with ISO C99/POSIX.1-2008 [closes issue #17]
* do not stamp time as OK when the minute is exactly one bit too long [closes
  issue #18]
* display last bit of the minute in dcf77pi-analyze [closes issue #19]
* allow setting the system time on UTC hosts [closes issue #20]
* fix crashes on bad input signal, return ehw\_random in this case [closes
  issue #21]
* reset DST and leap second announcement flags at the hour [closes issue #22]
* correctly handle bit buffer overflows [closes issue #26]
* fix parsing of log files when extra spaces are inserted [closes issue #25
  and #26]
* replace enumeration eGB\_skip by a boolean
* fix a bug where time could be all 0 if the previous minute was too long
* fix get\_isotime(): ISO days of year are 0-based, not 1-based
* also recover from too high values of bit 20
* dcf77pi: fix bug where the date parity but would only turn blue when the day
  of the month was wrong
* dcf77pi: improvements to statusbar timing and messages and "state" light
* update README.md on setting the system time

Version 3.6.0 -- 2017-05-14
* readpin: implement raw mode (parameter -r) to show the output of get\_pulse()
* README.md: add -r parameter of readpin
* add LICENSE.md [closes issue #5]
* add this change log [closes issue #14]
* default to "unknown third-party type" if the two third-party bits are unequal
* change century\_offset() to start add 1900 instead of 2000 so that
  transmissions from 1973 and onward can also be decoded
* do not clear tm\_sec in get\_isotime()
* show BCD errors in dcf77pi as blue parity bits
* allow running readpin and dcf77pi as regular user, add instructions [closes
  issue #9]
* do not bail out if writing to /sys/class/gpio/export on Linux fails
* split off the following calendar-related API from decode\_time.[ch] into
  calendar.[ch] [closes issue #13]
  * add\_minute()
  * century\_offset()
  * dayinleapyear[] (now public)
  * get\_dcftime()
  * get\_isotime()
  * get\_utchour()
  * get\_weekday (removed)
  * isleapyear() (now public)
  * lastday()
  * substract\_minute()
  * weekday[] (now public)
* split decode\_time() into smaller functions [closes issue #13]
* API overhaul to improve clarity by splitting up the bit masks into structures
  consisting of several finer-grained enumerations:
  * int, DT\_\* -> struct DT\_result, enum eDT\_\*
  * TPTYPE -> enum eTP
  * int, GETBIT\_\* -> struct GB\_result, enum eGB\_\*
  * BASEYEAR -> constant integer base\_year
  * BUFLEN -> private constant integer
* pass non-changing structs by value instead of double const reference
* pass array parameters as arrays, not as constant pointers
* consolidate variables modified by mainloop() into struct ML\_result
* add a value to struct DT\_result to indicate BCD errors
* only handle bit 15 when the sanity checks pass
* add Doxygen comments to all structures, enumeration and macros
* use mostly native int for variable types:
  * bool, char, short, int, and long long are all equal sized on armv6, amd64
    and i386
  * saves 1%-2% in binary sizes
  * requires less casts
  * no warnings from include-what-you-use 0.7 related to stdint.h
* unroll most macros
* use -1 instead of 0xFF for a bad minute length, adjust all related functions
* fix double free when opening the GPIO device on Linux fails
* add testcentury to 'make splint' and 'make all'
* resurrect 'make lint', splint fails on contemporary clang versions [issue #6]
* add 'make cppcheck' and 'make iwyu', other fixes [issue #6]
* fix build with GCC 4.9.2
* cppcheck, lint and iwyu (include-what-you-use) fixes
* add 'make test' which calls 'make testcentury' [issue #3]
* add 'make (un)install-md' to (un)install the .md files

Version 3.5.0 -- 2016-05-01
* rename isotime() to get\_isotime() and dcftime() to get\_dcftime() to not
  confuse Doxygen
* make lastday() and century\_offset() public for testcentury.c
* add a program testcentury to check the century calculations
* allow GPIO pin numbers > 255 (16 bits instead of 8) [closes issue #1, closes
  pull #2]
* improve README.md after a private e-mail discussion
* Makefile fixes

Version 3.4.2 -- 2016-01-03
* fix bug where century calculation would sometimes be wrong, affects version
  3.4.0 and 3.4.1

Version 3.4.1 -- 2015-12-26
* check if the DST flag is set within the expected date range, set DT\_DSTJUMP
  otherwise
* increase resilience against bit errors when checking for DST changes
* allow changing the DST flag when a parity flag is wrong
* do not set the DST state if DT\_DSTJUMP is set or if there is a generic error

Version 3.4.0 -- 2015-11-08
* detangle the third-party buffer from decode\_alarm()
* set the time in mainloop() instead of a frontend via set\_time(), add function
  get\_mainloop\_result() to report on setting the time
* display updates to dcf77pi
* clean up Makefile, install files with proper mode, sanitize uninstallation
* add Doxygen support to Makefile
* fix Linux build
* support multiple GPIO devices (FreeBSD only)
* write all initial error messages from input.c to stderr
* support older glibc
* rename write\_new\_logfile() to append\_logfile() to better describe its
  purpose
* cppcheck fixes, fix file descriptor leak
* replace lint with splint and support splint on Cygwin
* splint fixes
  * return 2 instead of GETBIT\_IO if get\_pulse() failed on IO errors
  * make sure bit.signal[] is allocated when writing to it
* set DST flag to "unknown" upon start, make this fatal for setclock()

Version 3.3.0 -- 2014-12-24
* fixes to the schematics file
* fix Linux build
* optimize API documentation for Doxygen
* re-introduce tunetime as part of get\_bit\_live()
* refresh acc\_minlen in dcf77pi each second instead of each minute
* fix bug where acc\_minlen would be truncated by decode\_time()
* move acc\_minlen API form decode\_time.[ch] to input.[ch]
* mostly synchronize the acc\_minlen behavior between live and file mode
* add acc\_minlen to each line of the log file, this can be read back with
  dcf77pi\_analyze
* add cutoff value bit.t / bit.realfreq to each line of the log file, adapt
  get\_bit\_file(), add function get\_cutoff() and add two new GETBIT flags
* show the cutoff value in dcf77pi and dcf77pi\_analyze
* rename process\_new\_minute() to process\_new\_second()
* skip consecutive end-of-minute markers in the log file instead of considering
  them as a valid bit
* also add the contents of the first full minute to the third-party buffer
* more fine-grained time/date corrections
* decode the alarm somewhat more and mention the main regions, with permission
  from Mr. Karl Wenzelewski from DIN
* add substract\_minute() to decode\_time.[ch]
* other API improvements
* fix bug where acc\_minlen is sometimes 1000 ms short when reading from a log
  file reaches the end

Version 3.2.0 -- 2014-09-16
* extract generic part of decode\_alarm.[ch] into bits1to14.[ch]
* reset lengths of bit0 and bit20 if the latter gets lower than the former
* add schematics of my receiver as a FidoCadJ file
* eliminate some warnings on Cygwin GCC
* remove redundant fields "a", "frac" and "maxone" from struct bitinfo
* convert all floating point operations to integer operations
* restrict ranges of some fields of struct hw
* convert bit.signal[] into a packed array, reducing its size 7.5 times
* add function setclock\_ok() to setclock.[ch]
* add functions dcftime() and isotime() to decode\_time.[ch]
* make acc\_minlen a first class citizen in decode\_time.[ch] and increase it
  by the actually measured bit length in ms when decoding live
* other small API fixes
* readpin: prevent time from temporarily going backwards
* readpin: clean up after ourselves when pressing Ctrl-C
* lint fixes
* updates to README.md and API documentation

Version 3.1.0 -- 2014-07-14
* change sleep time between getting pulses, realfreq now 6.5% more accurate
* do not adjust length of bit0 or bit20 in case of reception errors or
  end-of-minute
* improve robustness against thunderstorms
* ensure to clear DST/leap second announcement flags in case of a false
  positive (should not happen)
* introduce bit.tlast0, recording the last timestamp in a second where the
  average signal is 0, useful to measure signal quality or the algorithmic
  delay (~ 50 ms)
* introduce bit.signal, recording the raw radio signal of that bit
* readpin: drop tunetime, it was not part of the library and only gained ~1%
  on realfreq
* readpin: use getbit\_live() and next\_bit() from libdcf77 instead of
  duplicating code
* test for MacOS and Cygwin (non-live only)
* fix compatibility with GNU make
* update README.md

Version 3.0.0.1 -- 2014-06-01
* Fix parallel build and installation of libdcf77

Version 3.0.0 -- 2014-06-01 - merge lib branch
* split off the log analysis code (the -f parameter) into dcf77pi-analyze.c
* move core functionality into libdcf77.so :
  * receiving bits
  * reading and writing log files
  * decoding time
  * decoding German civil warning
  * setting the system clock
  * reading the configuration file
  * main loop functionality common to dcf77pi.c and dcf77pi-analyze.c, using
    callbacks to differentiate in functionality
* add Doxygen descriptions to functions, structures and other definitions of
  libdcf77
* move presentation functionality into dcf77pi.c or dcf77pi-analyze.c
* only dcf77pi.c uses ncurses, guifuncs.c is fully merged into it
* numerous interface cleanups
* install library and header files
* lint fixes
* update README.md

---

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

---

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
