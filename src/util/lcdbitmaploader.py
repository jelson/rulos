#!/usr/bin/python3

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


from PIL import Image
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

		fp.write("const uint8_t PROGMEM %s_data[] = {\n" % symname)
		imagebytes = self.image.tobytes()
		sys.stderr.write("num image bytes: %s %s\n" % (len(imagebytes), self.getcatsize()))
		fp.write("".join(map(lambda s: " 0x%02x,\n" % s, imagebytes)))
		fp.write("};\n")

		fp.write("const uint16_t PROGMEM %s_index_to_x[] = {\n" % symname)
		ary = map(lambda s: " %d,\n" % s, self.index_to_x)
		fp.write("".join(ary))
		fp.write("};\n")

		fp.write("const char PROGMEM %s_index_to_sym[] = {\n" % symname)
		fp.write("".join(map(lambda s: " '%c',\n" % s[0], self.index_to_sym)))
		fp.write("};\n")

		fp.write("uint8_t %s_num_syms = %d;\n" % (symname, len(self.index_to_sym)))
		fp.write("uint16_t %s_row_width_bytes = %d;\n" % (symname, self.getcatsize()[0]/8))
		fp.write("uint16_t %s_num_rows = %d;\n" % (symname, self.getcatsize()[1]/8))

		#self.image.save("catimage.png")
		
def main():
	print("globbing %s" % (os.path.abspath(sys.argv[2])))
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
