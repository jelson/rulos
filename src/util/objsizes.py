#!/usr/bin/python

# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson (jelson@gmail.com).
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

import glob
import os
import operator
import fnmatch

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
                for root, dirnames, filenames in os.walk(dir):
                        for filename in fnmatch.filter(filenames, "*.o"):
                                f = os.path.join(root, filename)
                                self.d[f] = Sizes(f)

	def dump(self):
		for (k,v) in self.d.items():
			print v
		print "grand total: %s" % self.grand_total()

	def grand_total(self):
		sizes = map(lambda s: s.total, self.d.values())
		return reduce(operator.add, sizes, 0)

if (__name__=="__main__"):
        for objdir in glob.glob("obj.avr.*"):
                print objdir
	        Collection(objdir).dump()
