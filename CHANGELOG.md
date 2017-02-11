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

Version 2.1.0 --

Version 2.0.3 --

Version 2.0.2 --

Version 2.0.1 --

Version 2.0.0 --
===============

* merge gui branch

Version 1.1.3 --

Version 1.1.2 --

Version 1.1.1 --

Version 1.1.0 --

Version 1.0.5 -- 2013-10-28
* update for stage support in FreeBSD
* compile with debug information
* add a "lint" target to check the code for both FreeBSD and Linux
* some "make lint" fixes
* readpin: substract the time it takes to get a pulse from the sleep time to
  bring the number of actual samples closer to the specified frequency, show
  the per-pulse sleep difference at the end of each second
* readpin: do not hardcode the hardware filename
* display different symbols for missing end-of-minute marker and too many bits
* also decode if the end-of-minute marker is missing
* also show the bit position and the state when the -v parameter is given
* show when the DST or leap second flags are erroneously set (not on the last
  Sunday or last day of a month), do not processs them in these cases
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
* do not insist on a mininum wall clock minute length when an end-of-minute
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
* introduce isleap() to avoid relying on the system clock
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
* allow time changes when all previously received minutes contained an error
* indicate receiver and transmitter errors when decoding the log file

Version 1.0.1 -- 2013-06-22
* add an "install" target to Makefile

Version 1.0.0 -- 2013-06-21
* initial release

Initial commit -- 2013-05-14
