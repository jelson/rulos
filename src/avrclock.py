#!/usr/bin/python

import os
import sys

FUSETABLE={
	'atmega8':		{1:0xe1, 2:0xe2, 4:0xe3, 8:0xe4},
	# atmega328p page 298, 33
	'atmega328p':	{8:0xe2},
	}

def main():
	try:
		mcu=sys.argv[1]
		speed=int(sys.argv[2])
		fuse=FUSETABLE[mcu][speed]
	except:
		sys.stderr.write("usage: %s atmega<8|328p> <speedInMHz>\n" % sys.argv[0])
		sys.exit(-1)

	cmd = "sudo avrdude -c usbtiny -p %s -u -U lfuse:w:0x%02x:m" % (mcu, fuse)
	os.system(cmd)

main()
