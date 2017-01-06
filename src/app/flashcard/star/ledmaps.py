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


import Image
import PIL.ImageOps
import glob
import sys
import os.path

class ImageCat:
	def __init__(self):
		self.arrays = []

	def catimage(self, filename):
		img = Image.open(filename)
		binary = PIL.ImageOps.invert(img.convert("RGB")).convert("1")
		#binary.save("binary.%s.png" % os.path.basename(filename))
		imagebytes = binary.tobytes()
		imagedata = "".join(map(lambda b: "0x%02x, " % ord(b), imagebytes))
		array = "{ %s }" % imagedata
		self.arrays.append(array)

	def emit(self, symname, filename):
		ofp = open(filename, "w")
		ofp.write("uint8_t %s[][8] = {\n" % symname);
		for array in self.arrays:
			ofp.write("\t%s,\n" % array);
		ofp.write("};\n");
	
def main():
	flist = glob.glob("%s/star*.png" % (sys.argv[2]))
	flist.sort()

	cat = ImageCat()
	for fn in flist:
		cat.catimage(fn)

	cat.emit("starbitmap", sys.argv[1])

main()


