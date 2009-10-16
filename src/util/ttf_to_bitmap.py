#!/usr/bin/python

import os
import glob
import sys
import PIL.Image
import PIL.ImageDraw
import PIL.ImageFont

basedir = sys.argv[1]

def TransformFont(zipfile, facename="*", yoff=-1, fontsize=8):
	base = zipfile.split(".")[0]
	os.system("rm -rf tmpdir")
	os.system("unzip -qq %s/eight_pixel_fonts/%s -d tmpdir" % (basedir,zipfile))
	fontfile = glob.glob("tmpdir/%s.[Tt][Tt][Ff]" % facename)[0]
	font = PIL.ImageFont.truetype(fontfile, fontsize)
	im = PIL.Image.new('1', (1024,8), 255)
	imd = PIL.ImageDraw.Draw(im)
	
	xIndexTable = []
	x = 0
	for c in range(32,127):
		xIndexTable.append(x)
		s = chr(c)
		(cx,cy) = imd.textsize(s, font=font)
		imd.text((x,yoff), s, font=font)
		x += cx
	width = x
	xIndexTable.append(width)
	xIndexTable.append(width)	# extra to deal with odd entry in widthpairs table
	#print "total width %s" % width
	im.save("%s/laserfont-samples/%s.png" % (basedir,base))
	os.system("rm -rf tmpdir")

	def getByte(x):
		v = 0
		for y in range(8):
			pixel = not (im.getpixel((x, y)) & 1)
			v = (v<<1) | pixel
		return v

	fp = sys.stdout
	fp.write("LaserFont font_%s = {\n" % base)
	fp.write("  {\n")
	for i in range(0,len(xIndexTable)-1,2):
		char_width0 = xIndexTable[i+1]-xIndexTable[i]
		assert(char_width0<16)
		char_width1 = xIndexTable[i+2]-xIndexTable[i+1]
		assert(char_width1<16)
		char_width = (char_width0<<4) | char_width1
		fp.write("    %d,\n" % char_width)
	fp.write("  },\n")
	fp.write("  {\n"),
	for x in range(width):
		fp.write("    %d,\n" % getByte(x))
	fp.write("  }\n")
	fp.write("};\n")

def main():
	
	try:
		os.mkdir("%s/laserfont-samples" % basedir)
	except:
		pass

#	TransformFont("c_c_red_alert", "*INET*", fontsize=12, yoff=-2)
#	TransformFont("d3cutebitmapism")
#	TransformFont("fff_atlantis")
#	TransformFont("graphicpixel")
#	TransformFont("leviwindows")
#	TransformFont("pf_ronda_seven", yoff=-3)
	TransformFont("uni_05_x")
#	TransformFont("ventouse")
#	TransformFont("visitor", "visitor1", fontsize=10, yoff=-1)
#	TransformFont("silkscreen", "slkscr")

main()
