#!/usr/bin/python
import Image
import glob

for ifn in glob.glob("orig/?.png"):
	ip = ifn.split('/')[-1].split(".")[0]
	im = Image.open(ifn)
	im2 = im.crop((9,0,25,24))
	im2.save(ip+".png")
