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

# LVGL, the graphics library RULOS display apps draw with. The ESP-IDF build
# gets it from Espressif's component registry; the host build clones the
# same release from GitHub so a UI renders identically on the PC.

import glob
import os
import re
import struct
import subprocess
from . import util
from SCons.Script import *

VERSION = "9.3.0"
REPO = "https://github.com/lvgl/lvgl.git"


def lvgl_root():
    root = os.path.abspath(os.path.join(util.BUILD_ROOT, "compilers", "lvgl", f"v{VERSION}"))
    if not os.path.exists(root):
        print(f"Cloning LVGL v{VERSION} into {root}")
        subprocess.check_call(
            ["git", "clone", "--depth", "1", "--branch", f"v{VERSION}", REPO, root]
        )
    return root


def _sdl2_available():
    return subprocess.run(["pkg-config", "--exists", "sdl2"]).returncode == 0


def LvglHostProgram(name, sources, lv_conf_dir=".", extra_cflags=[]):
    """Build a native program from LVGL plus the given sources, so an LVGL UI
    can be run on the PC. The program gets an SDL2 window when libsdl2-dev
    is installed; otherwise it is compiled with LV_USE_SDL 0 and can only do
    what it can do headless (e.g. render to a file).

    sources are relative to the SConstruct's directory and may reach into
    the RULOS library (e.g. "../../lib/periph/weather/weather.c").
    lv_conf_dir holds the lv_conf.h shared with the device build.
    """
    root = lvgl_root()
    build_dir = os.path.join(util.BUILD_ROOT, name, "host")

    # LVGL is hundreds of source files; don't make the first build serial.
    if GetOption("num_jobs") == 1:
        SetOption("num_jobs", os.cpu_count())

    scons_tools = os.path.join(os.path.dirname(__file__), "scons_tools")
    env = Environment(
        ENV={"PATH": os.environ["PATH"]},
        tools=["default", "spinner"],
        toolpath=[scons_tools],
        SPINNER_DISABLE=GetOption("verbose"),
    )
    env["SPINNER_PREFIX"] = "host"
    env.Append(
        CPPPATH=[root, os.path.abspath(lv_conf_dir), os.path.join(util.SRC_ROOT, "lib")],
        CPPDEFINES=["LV_CONF_INCLUDE_SIMPLE"],
        CCFLAGS=["-O1", "-g"],
        LIBS=["m"],
    )
    if _sdl2_available():
        env.ParseConfig("pkg-config --cflags --libs sdl2")
        env.Append(CPPDEFINES=[("LV_USE_SDL", 1)])
    else:
        print(f"{name}: libsdl2-dev not installed; building the host program without a window")

    # LVGL itself, under its own warning settings
    lvgl_build_dir = os.path.join(build_dir, "lvgl")
    env.VariantDir(lvgl_build_dir, root, duplicate=0)
    lvgl_sources = [
        os.path.join(lvgl_build_dir, os.path.relpath(p, root))
        for p in glob.glob(os.path.join(root, "src", "**", "*.c"), recursive=True)
    ]
    lvgl_lib = env.StaticLibrary(os.path.join(lvgl_build_dir, "lvgl"), lvgl_sources)

    # the app, under RULOS's
    app_env = env.Clone()
    app_env.Append(CCFLAGS=["-Wall", "-Werror"] + extra_cflags)
    app_build_dir = os.path.join(build_dir, "app")
    app_env.VariantDir(app_build_dir, util.PROJECT_ROOT, duplicate=0)
    app_sources = [
        os.path.join(app_build_dir, os.path.relpath(os.path.abspath(s), util.PROJECT_ROOT))
        for s in sources
    ]
    program = app_env.Program(os.path.join(build_dir, name), app_sources + lvgl_lib)
    Default(program)
    return program


# Embeds PNG files as LVGL image descriptors that LVGL decodes on first use
# (LV_USE_LODEPNG), so artwork stays in its original form and needs no
# converter. Writes one C source per PNG into out_dir and returns their
# paths; the descriptor for icons/clear-day.png is `<prefix>clear_day`,
# declared with LV_IMAGE_DECLARE() wherever it is used.
def LvglPngImages(png_paths, out_dir, prefix="img_"):
    os.makedirs(out_dir, exist_ok=True)
    sources = []
    for png_path in png_paths:
        name = re.sub(r"[^a-z0-9]", "_", os.path.splitext(os.path.basename(png_path))[0].lower())
        symbol = prefix + name
        source = os.path.join(out_dir, name + ".c")
        sources.append(source)
        if os.path.exists(source) and os.path.getmtime(source) >= os.path.getmtime(png_path):
            continue

        with open(png_path, "rb") as f:
            data = f.read()
        assert data[:8] == b"\x89PNG\r\n\x1a\n", f"{png_path} is not a PNG"
        width, height = struct.unpack(">II", data[16:24])
        rows = [
            "    " + " ".join(f"0x{b:02x}," for b in data[i : i + 16])
            for i in range(0, len(data), 16)
        ]
        with open(source, "w") as f:
            f.write(
                f"""// generated from {os.path.relpath(png_path, util.PROJECT_ROOT)}; do not edit
#include "lvgl.h"

static const uint8_t {symbol}_png[{len(data)}] = {{
{chr(10).join(rows)}
}};

// the declaration gives the definition external linkage in C++ too
LV_IMAGE_DECLARE({symbol});
const lv_image_dsc_t {symbol} = {{
    .header =
        {{
            .magic = LV_IMAGE_HEADER_MAGIC,
            .cf = LV_COLOR_FORMAT_RAW_ALPHA,
            .flags = 0,
            .w = {width},
            .h = {height},
            .stride = 0,
            .reserved_2 = 0,
        }},
    .data_size = {len(data)},
    .data = {symbol}_png,
}};
"""
            )
    return sources
