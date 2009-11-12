#!/usr/bin/python

x = open("silence.au")
y = x.read(24)
x.close()
for i in range(7993-24):
	y += ("%c"%255)
o = open("golden-silence.au", "w")
o.write(y)
o.close()
