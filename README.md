dcf77pi
=======

Yet another DCF77 decoder. This one is intended for the Raspberry Pi platform
but might work on other devices using GPIO pins too.

For the Raspberry Pi, connect a stand-alone DCF77 receiver to a GPIO
communication pin (default is 17) and the GPIO GND/3.3V pins (not 5V, this will
break the Raspberry Pi).

An example schematics of a receiver is shown in receiver.fcd which can be shown
using the FidoCadJ package.

The software comes with three binaries and a library:

* dcf77pi : Live decoding from the GPIO pins in interactive mode. Useable keys
  are shown at the bottom of the screen. The backspace key can be used to
  correct the last typed character of the input text (when changing the name of
  the log file). Depending on the permissions of your GPIO device node (the
  files in /dev), you might need to run this program as root or with sudo.
* dcf77pi-analyze filename : Decode from filename instead of the GPIO pins.
  Output is generated in report mode.
* readpin [-qr] : Program to test reading from the GPIO pins and decode the
  resulting bit. Send a SIGINT (Ctrl-C) to stop the program. Optional parameters
  are:
  * -q do not show the raw input, default is to show it.
  * -r raw mode, bypass the normal bit reception routine, default is to use it.
* libdcf77.so: The shared library containing common routines for reading bits
  (either from a log file or the GPIO pins) and to decode the date, time and
  third party buffer. Both dcf77pi and dcf77pi-analyze use this library. Header
  files to use the library in your own software are supplied.

The meaning of the keywords in config.txt is:

* pin           = GPIO pin number (0-65535)
* iodev         = GPIO device number (FreeBSD only)
* activehigh    = pulses are active high (1) or passive high (0)
* freq          = sample frequency in Hz (10-155000)
* summermonth   = month in which daylight saving time starts
* wintermonth   = month in which daylight saving time ends
* leapsecmonths = months (in UTC) in which a leap second might be inserted
* outlogfile    = name of the output logfile which can be read back using
  dcf77pi-analyze (default empty). The log file itself only stores the
  received bits, but not the decoded date and time.

Note that a value of 0 can be used for summermonth, wintermonth, or
leapsecmonths to disallow daylight saving time changes or leap seconds.
Otherwise use one or more values from 1 to 12.

The end of the minute is noted by the absence of high pulses. An absence of low
pulses probably means that the transmitter is out of range. Any other situation
will result in a logical read error.

With permission (comment 5916), the method described at
http://blog.blinkenlight.net/experiments/dcf77/binary-clock is used to receive
the bits.

Currently supporrted platforms:
* FreeBSD, Linux: full support
* Cygwin, MacOS, NetBSD: supported without GPIO communication for live decoding
* Windows: only via Cygwin

On Linux, you will have to install an (n)curses package development using your
package manager to get the required header files and the .so library file.
For example:
```sh
% sudo apt-get install ncurses-devel
```

To build and install the program into /usr/bin , the library into /usr/lib and
the configuration file into /usr/etc/dcf77pi :
```sh
% make PREFIX=/usr
% sudo make install PREFIX=/usr
```
