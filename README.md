dcf77pi
=======

Yet another DCF77 decoder. This one is intended for the Raspberry Pi platform
but might work on other devices using GPIO pins too.

For the Raspberry Pi, connect a stand-alone DCF77 receiver to the GPIO pin
(default is 17) and the GND/3.3V pins (not 5V, this will break the Pi).

Allowed parameters are:

* -f filename: read from filename instead of the GPIO pin.
* -l filename: log to filename when reading from the GPIO pin.
* -v         : verbose information when reading from the GPIO pin.

To stop the program, send a SIGINT (Ctrl-C) to it.

The meaning of the keywords in config.txt is:

* pin = GPIO pin number
* freq = sample frequency in Hz
* margin = margin for detection of bit values
* minlen = minimum length of a second expressed as percentage of the received samples
* maxlen = maximum length of a second expressed as percentage of the received samples
* activehigh = pulses are active high (1) or passive high (0)
* summermonth = month in which daylight saving time starts
* wintermonth = month in which daylight saving time ends
* leapsecmonths = months (in UTC) in which a leap second might be inserted

The margin can be used to adjust the valid ranges for '0' and '1' bits by
defining the allowed ranges of initial high pulses of each second:

* EOM tail -> [ .. margin %]
* 0 bit -> [ margin % .. (10 + margin) %]
* 1 bit -> [(20 - margin) % .. ]

The end of the minute is noted by the absence of high pulses. An absence of
low pulses means that the transmitter is out of range. Any other situation
will result in a logical read error.

To combat bad radio reception, pulses with holes (positive and negative) are
concatenated by insisting on a minimum pulse length.
