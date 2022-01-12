# Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
# (jelson@gmail.com).
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

import glob
import os
import subprocess
import sys

PROJECT_ROOT = os.path.relpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC_ROOT = os.path.join(PROJECT_ROOT, "src")
BUILD_ROOT = os.path.join(PROJECT_ROOT, "build")

def cwd_to_project_root(inp):
    if type(inp)==type([]):
        return [os.path.relpath(s, PROJECT_ROOT) for s in inp]
    else:
        assert(type(inp)==type(""))
        return os.path.relpath(inp, PROJECT_ROOT)

def parse_peripherals(peripherals):
    if type(peripherals)==type(""):
        return peripherals.split()
    assert(type(peripherals)==type([]))
    return peripherals

def die(msg):
    sys.stderr.write(msg+"\n")
    sys.exit(1)

def cglob(*kargs):
    retval = []
    for ext in ["*.c", "*.cpp"]:
        globpath = tuple(list(kargs) + [ext])
        retval.extend(cwd_to_project_root(glob.glob(os.path.join(*globpath))))
    return retval

def template_free_cglob(*kargs):
    return [f for f in cglob(*kargs) if not f.endswith("_template.c")]

def pkgconfig(mode, package):
    return subprocess.check_output(["pkg-config", mode, package]).decode("utf-8").split()
