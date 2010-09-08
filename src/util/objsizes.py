#!/usr/bin/python

#
# This file is part of RulOS, Ravenna Ultra-Low-Altitude Operating
# System -- the free, open-source operating system for microcontrollers.
#
# Written by Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com),
# May 2009.
#
# This operating system is in the public domain.  Copyright is waived.
# All source code is completely free to use by anyone for any purpose
# with no restrictions whatsoever.
#
# For more information about the project, see: www.jonh.net/rulos
#

import glob
import os
import operator

class Sizes:
	def __init__(self, objfilename):
		self.objfilename = objfilename
		self.d = {}
		self.total = 0
		fp = os.popen("nm -S "+objfilename, "r")
		while (True):
			l = fp.readline()
			if (l==""): break
			fields = l.rstrip().split()
			if (len(fields)>=4 and fields[2]=="T"):
				size = int(fields[1], 16)
				self.d[fields[3]] = size
				self.total += size
		fp.close()

	def __repr__(self):
		return "%s: %s" % (self.objfilename, self.total)

class Collection:
	def __init__(self, dir):
		self.d = {}
		for f in glob.glob(dir+"/*.o"):
			self.d[f] = Sizes(f)

	def dump(self):
		for (k,v) in self.d.items():
			print v
		print "grand total: %s" % self.grand_total()

	def grand_total(self):
		sizes = map(lambda s: s.total, self.d.values())
		return reduce(operator.add, sizes, 0)

if (__name__=="__main__"):
	Collection("obj.avr").dump()
