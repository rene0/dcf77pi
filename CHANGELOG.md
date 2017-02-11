Version 4.0.0 -- future release
===============================

* merge interrupt branch

Version 3.6.0 -- 
----------------

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

Version 1.0.5 --

Version 1.0.4 --

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
* readpin: display pulses as characters
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
