dcf77pi
=======

Yet another DCF77 decoder. This one is intended for the Raspberry Pi platform
but might work on other devices using GPIO pins too.

For the Raspberry Pi, connect a stand-alone DCF77 receiver to the GPIO pin
(default is 17) and the GND/3.3V pins (not 5V, this will break the Pi).

The format for hardware.txt is:
  sample frequency in Hz
  margin for detection of bit values
  GPIO pin number

The margin can be used to adjust the valid ranges for '0' and '1' bits by
defining the allowed ranges of initial high pulses of each second:
0 bit -> [(10 - margin) % .. (10 + margin) %]
1 bit -> [(10 - margin) % .. (10 + margin) %]

The end of the minute is noted by the absence of high pulses.
Any other situation will result in a logical read error.

A high pulse after a low pulse always starts a new second, even when in
the middle of a second in case of bad radio reception.
