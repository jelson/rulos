#!/usr/bin/python

import glob
import commands

for f in glob.glob('*/src/*.c'):
    h = f.split("/")[-1].split(".")[0]
    d = f.split("/")[0]

    cmd = "svn mv ../../include/%s.h %s/inc" % (h, d)

    print cmd
    print commands.getoutput(cmd)

    
