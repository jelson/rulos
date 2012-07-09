#!/usr/bin/python

# Cats a file out to the serial port, to look like the data we captured
# off a GPS (without hanging the GPS outside.)

import sys

class GPSSim:
	def __init__(self):
		self.serial = open("/dev/ttyUSB1", "w")
#		self.copy_file()
		self.synthesize()

	def send_gps(self, s):
		if (self.serial!=None):
			count = self.serial.write(s)
#			if (count!=len(s)): raise Exception("serial port broke %s!=%s" % (len(s), count))
		sys.stdout.write(s)
	

	def copy_file(self):
		serial = open("/dev/ttyUSB1", "w")
		while (True):
			fp = open("test-gps-data", "r")
			while (True):
				c = fp.read(1)
				if (c==''): break
				self.send_gps(c)
			fp.close()

	def interp(self, p, t0, t1):
		q = 1.0-p
		return (t0[0]*q + t1[0]*p, t0[1]*q + t1[1]*p)

	def emit_one(self, pos):
		def toddm(deg):
			intdeg = int(deg)
			fracdeg = deg - intdeg
			minutes = fracdeg * 60
			return (intdeg, minutes)
		(xdeg,xmin) = toddm(-pos[0])
		(ydeg,ymin) = toddm(pos[1])
		str = "$GPGGA,235453,%d%06.3f,N,%d%06.3f,W,1,05,2.2,104.1,M,-18.4,M,,*7A\n" % (
			ydeg, ymin, xdeg, xmin)
		self.send_gps(str)

	def synthesize(self):
		p0 = (-122.341817+.00005, 47.672009-.00005)
		p1 = (-122.341966-.00005, 47.672792+.00005)
		steps = 100
		for i in range(steps+10):
			p = i/float(steps)
			pos = self.interp(p, p0, p1);
			self.emit_one(pos)
		
GPSSim()

