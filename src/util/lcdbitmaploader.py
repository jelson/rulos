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
	def __init__(self, size):
		if (size!=None):
			self.image = Image.new("1", size)
		else:
			self.image = None
		self.catsize = (0,0)
		self.symbol_index = 0
		self.index_to_sym = []
		self.index_to_x = []

	def getcatsize(self):
		x = self.catsize[0]
		y = self.catsize[1]
		x = (x+7) & ~7
		return (x,y)

	def catimage(self, filename):
		self.index_to_sym.append(os.path.basename(filename))
		self.index_to_x.append(self.catsize[0])
		self.symbol_index += 1
		img = Image.open(filename)
		bbox = PIL.ImageOps.invert(img.convert("RGB")).getbbox()
		left = max(0, bbox[0] - 1)
		right = min(img.size[0], bbox[2] + 1)
		img = img.crop((left, 0, right, img.size[1]))
		isize = img.size
		if (self.image!=None):
			self.image.paste(img, (self.catsize[0], 0))

		self.catsize = (self.catsize[0]+isize[0], max(self.catsize[1], isize[1]))
	
	def emit(self, symname, filename):
		self.index_to_x.append(self.catsize[0])
		fp = open(filename, "w")

		fp.write("uint8_t PROGMEM %s_data[] = {\n" % symname)
		imagebytes = self.image.tostring()
		sys.stderr.write("num image bytes: %s %s\n" % (len(imagebytes), self.getcatsize()))
		fp.write("".join(map(lambda s: " 0x%02x,\n" % ord(s), imagebytes)))
		fp.write("};\n")

		fp.write("uint16_t PROGMEM %s_index_to_x[] = {\n" % symname)
		ary = map(lambda s: " %d,\n" % s, self.index_to_x)
		fp.write("".join(ary))
		fp.write("};\n")

		fp.write("char PROGMEM %s_index_to_sym[] = {\n" % symname)
		fp.write("".join(map(lambda s: " '%c',\n" % s[0], self.index_to_sym)))
		fp.write("};\n")

		fp.write("uint8_t %s_num_syms = %d;\n" % (symname, len(self.index_to_sym)))
		fp.write("uint16_t %s_row_width_bytes = %d;\n" % (symname, self.getcatsize()[0]/8))
		fp.write("uint16_t %s_num_rows = %d;\n" % (symname, self.getcatsize()[1]/8))

		self.image.save("catimage.png")
		
def main():
	flist = glob.glob("%s/*.png" % (sys.argv[2]))
	flist.sort()

	# scan for size
	cat = ImageCat(None)
	for fn in flist:
		cat.catimage(fn)
	# now build actual image strip
	cat = ImageCat(cat.getcatsize())
	for fn in flist:
		cat.catimage(fn)

	cat.emit("lcd", sys.argv[1])

main()

