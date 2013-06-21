dcf77pi
=======

Yet another DCF77 decoder. This one is intended for the Raspberry Pi platform
but might work on other devices using GPIO pins too.

For the Raspberry Pi, connect a stand-alone DCF77 receiver to the GPIO pin
(default is 17) and the GND/3.3V pins (not 5V, this will break the Pi).

Allowed parameters are:
-f filename: read from filename instead of the GPIO pin.
-l filename: log to filename when reading from the GPIO pin.
-v         : verbose information when reading from the GPIO pin.

To stop the program, send a SIGINT (Ctrl-C) to it.

The format for hardware.txt is:
  sample frequency in Hz
  margin for detection of bit values
  GPIO pin number
  minimum length of a second expressed as percentage of the received samples
  active high (1) or passive high (low)

The margin can be used to adjust the valid ranges for '0' and '1' bits by
defining the allowed ranges of initial high pulses of each second:
0 bit -> [(10 - margin) % .. (10 + margin) %]
1 bit -> [(10 - margin) % .. (10 + margin) %]

The end of the minute is noted by the absence of high pulses. An absence of
low pulses means that the transmitter is out of range. Any other situation
will result in a logical read error.

Currently a high pulse after a low pulse always starts a new second,
even when in the middle of a second in case of bad radio reception.
