#!/usr/bin/python

import sys
sys.path.append("/usr/share/inkscape/extensions")
import os
from copy import deepcopy
import time
import inkex
import simplepath
import simplestyle
import cspsubdiv
import cubicsuperpath
import math
import random
import operator
import lxml.etree as etree
from lxml.builder import E
import shapely.geometry as shgeo


import os

class ScopeTrace:
	def __init__(self):
		self.samples = []

	def line(self, p0, p1, nsamples):
		for x in range(int(nsamples)):
			self.samples.append(self.interpolate(p0, p1, float(x)/nsamples))

	def interpolate(self, p0, p1, frac):
		return ((p1[0]-p0[0])*frac+p0[0],
				(p1[1]-p0[1])*frac+p0[1])

	def emit(self):
		str = ""
		for sample in self.samples:
			str += chr(int(sample[0]))
			str += chr(int(sample[1]))
		return str
	
	def minmax(self):
		def min(a,b):
			if (a==None):
				return b
			if (a<b):
				return a
			return b
		def max(a,b):
			if (a==None):
				return b
			if (a>b):
				return a
			return b
		mins = (None, None)
		maxs = (None, None)
		for sample in self.samples:
			mins = (min(mins[0], sample[0]), min(mins[1], sample[1]))
			maxs = (max(maxs[0], sample[0]), max(maxs[1], sample[1]))
		return ((mins[0], maxs[0]), (mins[1], maxs[1]))

	def rescale(self, torange):
		(xr, yr) = self.minmax()
		def scale(scalar, fromrange, torange):
			frac = (scalar-fromrange[0])/(fromrange[1]-fromrange[0])
			return torange[0] + (torange[1]-torange[0])*frac
		out = []
		for sample in self.samples:
			out.append(
				(scale(sample[0], xr, (torange[1],torange[0])), scale(sample[1], yr, torange)))
		newst = ScopeTrace()
		newst.samples = out
		return newst

class XYPaint(inkex.Effect):
	def __init__(self, *args, **kwargs):
		inkex.Effect.__init__(self)

	def effect(self):
		self.st = ScopeTrace()
		self.dpu = 0
		self.traverse()
		(xr, yr) = self.st.minmax()

		avgsize = (xr[1]-xr[0]+yr[1]-yr[0])/2.
		self.dpu = 150./avgsize

		self.st = ScopeTrace()
		self.traverse()
		self.finalize()

	def traverse(self):
		self.svgpath = inkex.addNS('path', 'svg')
		for id, node in self.selected.iteritems():
			path = simplepath.parsePath(node.get("d"))
			curpt = None
			for (type,point) in path:
				#sys.stderr.write("type=%s\n" % type)
				pointcopy = list(point)
				while (len(pointcopy)>0):
					pt = (pointcopy[0],pointcopy[1])
					pointcopy = pointcopy[2:]
					if (curpt==None):
						curpt = pt
					else:
						self.drawline(curpt, pt)
						curpt = pt

	def drawline(self, p0, p1):
		dx = p1[0]-p0[0]
		dy = p1[1]-p0[1]
		dist = math.sqrt(dx*dx+dy*dy)
		self.st.line(p0, p1, dist*self.dpu+3.0);

	def finalize(self):
		fn = "trace.pcm8"
		ofp = open(fn, "w")
		self.st = self.st.rescale((5,250))
		sys.stderr.write("minmax: %s\n" % (self.st.minmax(),))
		sys.stderr.write("nsamples: %s\n" % len(self.st.samples))
		s = self.st.emit()
		for i in range(20000):
			ofp.write(s)
		ofp.close()
		cmd = "play -t raw --rate 221000 --encoding unsigned-integer --bits 8 --channels 2 %s" % fn
		ofp = open("playcmd.sh", "w")
		ofp.write(cmd+"\n")
		ofp.close()

#def main():
#	st = ScopeTrace()
#	#st.line((0,255), (255, 0), 250)
#	st.line((10,10), (128, 240), 250)
#	st.line((128, 240), (240, 10), 250)
#	st.line((240, 10), (0, 0), 250)
#	#st.line((60, 140), (10,10), 250)
#	#st.line((240, 0), (10, 10), 250)

if __name__ == '__main__':
	e = XYPaint()
	e.affect()

