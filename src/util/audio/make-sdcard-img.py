#!/usr/bin/env python3

import subprocess
import shutil
import os
import re

source_dir = "sdcard"
img_filename = "sdcard.img"
mount_point = "sdcard.mount"

def unlink(path):
    try:
        os.unlink(path)
    except FileNotFoundError:
        pass
    assert not os.path.exists(path)

def sdcard_space_kb():
    du_result,_ = subprocess.Popen(["du", "-sk", source_dir], stdout=subprocess.PIPE).communicate()
    mo = re.search("(\d+)\t", du_result.decode("utf-8"))
    return int(mo.groups(1)[0])

def make_empty_image(img_filename, space_kb):
    unlink(img_filename)
    subprocess.call(["dd", "if=/dev/zero", "bs=1024", "count=%d" % space_kb, "of="+img_filename])

def cleanup():
    subprocess.call(["umount", mount_point])
    shutil.rmtree(mount_point)

def main():
    try:
        shutil.rmtree(mount_point)
    except FileNotFoundError:
        pass
    os.mkdir(mount_point)
    space_kb = sdcard_space_kb() + 100; # extra capacity for FS overhead, inodes
    make_empty_image(img_filename, space_kb)
    subprocess.call(["mkfs.fat", img_filename])
    subprocess.call(["mount", "-o", "loop", img_filename, mount_point])
    dummy_target = os.path.join(mount_point, "dummy")
    shutil.copytree(source_dir, dummy_target)
    cleanup()

main()
