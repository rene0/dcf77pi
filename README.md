dcf77pi
=======

Yet another DCF77 decoder. This one is intended for the Raspberry Pi platform
but might work on other devices using GPIO pins too.

For the Raspberry Pi, connect a stand-alone DCF77 receiver to the GPIO pin
(default is 17) and the GND/3.3V pins (not 5V, this will break the Pi).

Allowed parameters for dcf77pi are:

* -f filename: read from filename instead of the GPIO pin. If used, output is
  generated in report mode. Otherwise, interactive mode is used, with useable
  keys shown at the bottom of the screen.

Allowed parameters for readpin are:

* -t : subtract the pulse acquisition period from the time to sleep

To stop readpin or dcf77pi in report mode, send a SIGINT (Ctrl-C) to it.

The meaning of the keywords in config.txt is:

* pin = GPIO pin number
* activehigh = pulses are active high (1) or passive high (0)
* freq = sample frequency in Hz
* maxzero = maximum percentage of time the signal can be high for a 0 bit
* maxone = maximum percentage of time the signal can be high for a 1 bit
* summermonth = month in which daylight saving time starts
* wintermonth = month in which daylight saving time ends
* leapsecmonths = months (in UTC) in which a leap second might be inserted
* outlogfile = name of the output logfile which can be read back with the -f
  parameter (default empty).

Note that a sentinel value of 0 can be used for summermonth, wintermonth, or
leapsecmonths to disallow daylight saving time changes or leap seconds.

The end of the minute is noted by the absence of high pulses. An absence of
low pulses probably means that the transmitter is out of range. Any other
situation will result in a logical read error.

With permission (comment 5916), the method described at
http://blog.blinkenlight.net/experiments/dcf77/binary-clock is used to
receive the bits.
