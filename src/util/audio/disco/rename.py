#!/usr/bin/python

import glob
import os
import sys

SPACER="_"
def crunch(name):
	out = "_"
	for x in name:
		if (x.isalpha()):
			out+=x.lower()
		elif (out[-1]!=SPACER):
			out+=SPACER
	return out[1:]

for fn in glob.glob("*.ogg"):
	outname = crunch(fn[:-4])+".ogg"
	if (not outname.startswith("sound_")):
		outname = "sound_"+outname
	os.rename(fn, outname)
	sys.stderr.write("%s %s\n" % (fn, outname))
