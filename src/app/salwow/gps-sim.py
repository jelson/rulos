#!/usr/bin/python

# Cats a file out to the serial port, to look like the data we captured
# off a GPS (without hanging the GPS outside.)

import sys

serial = open("/dev/ttyUSB1", "w")
while (True):
	fp = open("test-gps-data", "r")
	while (True):
		c = fp.read(1)
		if (c==''): break
		serial.write(c)
		sys.stdout.write(c)
	fp.close()
