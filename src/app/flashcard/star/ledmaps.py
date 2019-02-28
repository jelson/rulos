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


from PIL import Image
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


