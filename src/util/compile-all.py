#!/usr/bin/env python3

import sys
import os
import glob
import subprocess

all_scons_files = glob.glob("../app/**/SConstruct", recursive=True)

def test_compile(sconsfile):
    sys.stdout.write("%-30s ... " % (os.path.dirname(sconsfile)))
    sys.stdout.flush()

    retval = subprocess.run(
        args=["scons", "-j", "25"],
        cwd=os.path.dirname(sconsfile),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

    if retval.returncode == 0:
        sys.stdout.write("passed\n")
    else:
        sys.stdout.write("FAIL\n")
    sys.stdout.flush()

for s in sorted(all_scons_files):
    if 'graveyard' in s:
        continue
    test_compile(s)
    
