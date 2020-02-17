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
import glob
import sys

class Index:
	def __init__(self, sym, offset_bits):
		self.sym = sym
		self.len = 0
		self.offset_bits = offset_bits

	def __repr__(self):
		return "{'%s', %d, %d}" % (self.sym, self.len, self.offset_bits)

def get_bitfield(bytes, offset, len):
	print("GB offset %d len %d" % (offset, len))
	ov = 0
	bitsAvaiableThisByte = 8-(offset%8)
	bitsUsedThisByte = min(bitsAvaiableThisByte, len)
	v = bytes[int(offset / 8)] >> (bitsAvaiableThisByte - bitsUsedThisByte)
	v &= ((1<<bitsUsedThisByte)-1)
	print("GB select value %03x from byte %d, right-shift %d" % (
		v, offset / 8, (bitsAvaiableThisByte - bitsUsedThisByte)))
	ov |= (v << (len-bitsUsedThisByte))

	rest_len = len - bitsUsedThisByte
	rest_offset = offset + bitsUsedThisByte
	print("GB rest_len %s rest_offset %s" % (rest_len, rest_offset))
	if (rest_len > 0):
		assert(rest_offset > offset)
		assert((rest_offset % 8)==0)
		ov |= get_bitfield(bytes, rest_offset, rest_len)
	return ov

class DataBlock:
	def __init__(self):
		self.index = {}
		self.data = []
		self.offset_bits = 0

	def emit(self, fp):
		fp.write("const RasterIndex rasterIndex[] = {\n")
		idxs = list(self.index.items())
		idxs.sort()
		for (k,idx) in idxs:
			fp.write("  %s,\n" % idx)
		fp.write("  {'\\0', 0, 0},\n")
		fp.write("};\n")
		fp.write("const uint8_t rasterData[] PROGMEM = {\n")
		for c in self.data:
			fp.write("  0x%02x,\n" % c)
		fp.write("};\n");

	def start_sym(self, sym):
		print("SS sim %s offset_bits %d\n" % (sym, self.offset_bits))
		self.index[sym] = Index(sym, self.offset_bits)

	def add_field(self, sym, elevenBitField):
		self.index[sym].len += 1
		bitsToGo = 11
		print("AF ---- start %03x; data offset %d" % (elevenBitField, self.offset_bits))
		while (bitsToGo > 0):
			openBits = 8 - self.offset_bits % 8
			if (openBits == 8):
				print("AF inserted a 0 byte")
				self.data.append(0)
				openBits = 8
			
			print("AF bitsToGo %d openBits %d" % (bitsToGo, openBits))
			# how many bits we'll append on this loop iteration
			valueWidth = min(openBits, bitsToGo)

			value = elevenBitField
			if (bitsToGo > openBits):
				value >>= (bitsToGo-openBits)
				print("AF unmasked value %03x" % value)
			value &= ((1<<valueWidth)-1)
			print("AF inserting val %01x width %d" % (value, valueWidth))

			if (openBits > bitsToGo):
				value <<= (openBits-bitsToGo)
				print("AF shifts value left %d" % (openBits-bitsToGo))
			self.data[-1] |= value
			print("AF data[%d]  %02x" % ((len(self.data)-1),self.data[-1]))
			bitsToGo -= valueWidth
			self.offset_bits += valueWidth

		gotValue = get_bitfield(self.data, self.index[sym].offset_bits
			+ (self.index[sym].len-1)*11, 11)
		if (gotValue != elevenBitField):
			print("put: %03x" % elevenBitField)
			print("got: %03x" % gotValue)
			assert(False)

class Stripe:
	def __init__(self, y, x0, x1):
		self.y = y
		self.x0 = x0
		self.x1 = x1

	def makeElevenBitField(self, lasty):
		v = 0
		if (lasty!=self.y):
			v |= (1<<10)
		v |= (self.x0<<5)
		v |= (self.x1<<0)
		return v

	def __repr__(self):
		return "{%d,%d,%d}" % (self.y, self.x0, self.x1)

class BitmapFromFile:
	def __init__(self, filename, datablock):
		self.symname = filename.split("/")[-1].split(".")[0][0]
		self.datablock = datablock
		self.datablock.start_sym(self.symname)
		self.startBlack = None
		self.lastStripe = None
		img = Image.open(filename)
		for y in range(img.size[1]):
			#print "y=%s" % y
			last = (255,255,255,255)
			for x in range(img.size[0]):
				pixel = img.getpixel((x,y))
				if (pixel!=last):
					self.flip(x,y)
				last = pixel
			if (self.startBlack):
				self.flip(x,y)

	def flip(self, x, y):
		m = "sf %s flip %s %s" % (self.startBlack, x,y)
		if (self.startBlack == None):
			self.startBlack = (x,y)
			#print m,"start"
		elif (y!=self.startBlack[1]):
			assert(False)
			return
		else:
			print(m,"accept")
			lasty = 0
			if (self.lastStripe!=None):
				lasty = self.lastStripe.y
			stripe = Stripe(y, self.startBlack[0], x)
			self.lastStripe = stripe
			ebf = stripe.makeElevenBitField(lasty)
			print("BFF lasty %s y %s stripe %s %s" % (lasty, y, stripe, ebf>>10))
			self.datablock.add_field(self.symname, ebf)
			self.startBlack = None

	def emit(self, fp):
		fp.write('  "%s", {\n' % self.symname)
		for stripe in self.stripes:
			fp.write("    %s,\n" % stripe)

def main():
	fp = open(sys.argv[1], "w")
	flist = glob.glob("%s/*.png" % (sys.argv[2]))
	flist.sort()
	datablock = DataBlock()
	for fn in flist:
		print("emitting for %s" % fn)
		bmp = BitmapFromFile(fn, datablock)
	datablock.emit(fp)
	fp.close()

main()
