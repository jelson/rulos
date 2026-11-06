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


# Converts the frames of pre-rendered sprite shows (see
# src/lib/periph/sprite_show/sprite_show.h) from the manifest a renderer
# wrote (manifest.json next to its PNGs: {"fps", "shows": [{"name",
# "images": [png paths], "frames": [[image, x, y], ...]}]}) into:
#
#   <prefix>frames.bin   every image 8-bit indexed with an alpha palette
#                        and RLE compressed, in the form LVGL's binary
#                        decoder takes (so the sprite player and LVGL
#                        agree on it), packed 4-byte aligned into one blob
#   <prefix>shows.c/.h   the show tables, with image offsets into the blob,
#                        as `const sprite_show_t <prefix>shows[]` and
#                        `<prefix>show_count`
#   <prefix>frames.S     the blob embedded in a program, as the symbol
#                        `<prefix>frames_blob`, for builds that keep it
#                        in the image rather than in a flash partition
#
# Returns a SpriteShows naming those files. Nothing is rewritten while the
# outputs are newer than the manifest.
class SpriteShows:
    def __init__(self, out_dir, prefix):
        self.blob = os.path.join(out_dir, prefix + "frames.bin")
        self.source = os.path.join(out_dir, prefix + "shows.c")
        self.header = os.path.join(out_dir, prefix + "shows.h")
        self.embed_source = os.path.join(out_dir, prefix + "frames.S")
        self.include_dir = out_dir


def LvglSpriteShows(manifest_path, out_dir, prefix="sprite_"):
    import json

    os.makedirs(out_dir, exist_ok=True)
    result = SpriteShows(out_dir, prefix)
    outputs = [result.blob, result.source, result.header, result.embed_source]
    if all(
        os.path.exists(o) and os.path.getmtime(o) >= os.path.getmtime(manifest_path)
        for o in outputs
    ):
        return result

    with open(manifest_path) as f:
        manifest = json.load(f)
    manifest_dir = os.path.dirname(manifest_path)
    print(f"converting sprite shows from {os.path.relpath(manifest_path, util.PROJECT_ROOT)}")

    blob = bytearray()
    shows_c = []
    stored = {}  # by PNG content, so a pose shared by shows is stored once
    for show in manifest["shows"]:
        images = []
        for rel in show["images"]:
            with open(os.path.join(manifest_dir, rel), "rb") as f:
                png = f.read()
            if png not in stored:
                data = _lvgl_indexed_rle(os.path.join(manifest_dir, rel))
                while len(blob) % 4:
                    blob.append(0)
                stored[png] = (len(blob), len(data["bin"]), data["width"], data["height"])
                blob += data["bin"]
            images.append(stored[png])
        name = re.sub(r"[^a-z0-9]", "_", show["name"].lower())
        sym = prefix + name
        shows_c.append(
            f"static const sprite_image_t {sym}_images[] = {{\n"
            + "".join(f"    {{{o}, {s}, {w}, {h}}},\n" for o, s, w, h in images)
            + "};\n"
            f"static lv_image_dsc_t {sym}_dscs[{len(images)}];\n"
            f"static const sprite_frame_t {sym}_frames[] = {{\n"
            + "".join(f"    {{{i}, {x}, {y}}},\n" for i, x, y in show["frames"])
            + "};\n"
        )
    table = "".join(
        f'    {{"{show["name"]}", {prefix}{re.sub(r"[^a-z0-9]", "_", show["name"].lower())}_images, '
        f"{len(show['images'])}, "
        f'{prefix}{re.sub(r"[^a-z0-9]", "_", show["name"].lower())}_frames, '
        f"{len(show['frames'])}, {manifest['fps']}, "
        f'{prefix}{re.sub(r"[^a-z0-9]", "_", show["name"].lower())}_dscs}},\n'
        for show in manifest["shows"]
    )
    generated = f"// generated by LvglSpriteShows() from {os.path.relpath(manifest_path, util.PROJECT_ROOT)}; do not edit"

    with open(result.blob, "wb") as f:
        f.write(blob)
    with open(result.header, "w") as f:
        f.write(f"""{generated}
#pragma once

#include "periph/sprite_show/sprite_show.h"

extern const sprite_show_t {prefix}shows[];
extern const unsigned {prefix}show_count;

// the blob's symbol when it is embedded with {os.path.basename(result.embed_source)}
extern const uint8_t {prefix}frames_blob[];
""")
    with open(result.source, "w") as f:
        f.write(f"""{generated}
#include "{os.path.basename(result.header)}"

{"".join(shows_c)}
const sprite_show_t {prefix}shows[] = {{
{table}}};
const unsigned {prefix}show_count = {len(manifest["shows"])};
""")
    with open(result.embed_source, "w") as f:
        f.write(f"""// {generated[3:]}
    .section .rodata
    .balign 4
    .global {prefix}frames_blob
{prefix}frames_blob:
    .incbin "{result.blob}"
""")
    return result


# An LVGL binary image in the form the bin decoder takes from memory: the
# 12-byte compression header (method, compressed size, decompressed size)
# followed by the RLE stream of the palette (256 BGRA entries) and the
# 8-bit index rows. The palette gives fully opaque colors most of its
# entries; the antialiased edge pixels share a few colors at seven alpha
# levels.
def _lvgl_indexed_rle(png_path):
    from PIL import Image

    OPAQUE_COLORS = 199
    EDGE_COLORS = 8
    ALPHA_LEVELS = 7
    EDGE_BASE = 1 + OPAQUE_COLORS

    img = Image.open(png_path).convert("RGBA")
    width, height = img.size
    pixels = list(img.getdata())
    opaque = [p[:3] for p in pixels if p[3] == 255]
    edge = [p[:3] for p in pixels if 0 < p[3] < 255]

    def quantize(colors, count):
        """Returns (palette as list of RGB, index of each color)."""
        if not colors:
            return [], {}
        strip = Image.new("RGB", (len(colors), 1))
        strip.putdata(colors)
        q = strip.quantize(colors=min(count, len(set(colors))), method=Image.Quantize.MEDIANCUT)
        pal = q.getpalette()[: 3 * (max(q.getdata()) + 1)]
        palette = [tuple(pal[i : i + 3]) for i in range(0, len(pal), 3)]
        return palette, dict(zip(colors, q.getdata()))

    opaque_palette, opaque_index = quantize(opaque, OPAQUE_COLORS)
    edge_palette, edge_index = quantize(edge, EDGE_COLORS)

    palette = [(0, 0, 0, 0)] * 256
    for i, (r, g, b) in enumerate(opaque_palette):
        palette[1 + i] = (b, g, r, 255)
    for c, (r, g, b) in enumerate(edge_palette):
        for level in range(ALPHA_LEVELS):
            palette[EDGE_BASE + c * ALPHA_LEVELS + level] = (
                b,
                g,
                r,
                (level + 1) * 255 // ALPHA_LEVELS,
            )

    indices = bytearray(width * height)
    for i, (r, g, b, a) in enumerate(pixels):
        if a == 255:
            indices[i] = 1 + opaque_index[(r, g, b)]
        elif a > 0:
            level = max(1, min(ALPHA_LEVELS, round(a * ALPHA_LEVELS / 255)))
            indices[i] = EDGE_BASE + edge_index[(r, g, b)] * ALPHA_LEVELS + level - 1
    raw = bytes(c for entry in palette for c in entry) + bytes(indices)
    rle = _rle_encode(raw)
    header = struct.pack("<III", 1, len(rle), len(raw))  # LV_IMAGE_COMPRESS_RLE
    return {"bin": header + rle, "width": width, "height": height}


# LVGL's RLE for 1-byte pixels: a control byte under 0x80 repeats the next
# byte that many times; one with the high bit set is followed by that many
# (low 7 bits) literal bytes.
def _rle_encode(raw):
    out = bytearray()
    literals = bytearray()

    def flush():
        for i in range(0, len(literals), 127):
            chunk = literals[i : i + 127]
            out.append(0x80 | len(chunk))
            out.extend(chunk)
        literals.clear()

    i = 0
    n = len(raw)
    while i < n:
        run = 1
        while i + run < n and raw[i + run] == raw[i] and run < 127:
            run += 1
        if run >= 3:
            flush()
            out.append(run)
            out.append(raw[i])
            i += run
        else:
            literals.append(raw[i])
            i += 1
    flush()
    return bytes(out)


def montserrat_ttf():
    """Montserrat Medium, the face LVGL's built-in fonts are made from (OFL)."""
    return os.path.join(lvgl_root(), "scripts", "built_in_font", "Montserrat-Medium.ttf")


# Renders a TTF into an LVGL bitmap font, as lv_font_conv does but with
# Pillow's FreeType rasterizer: a C source defining `const lv_font_t
# <name>` with 4-bit antialiased glyphs for the given code points, written
# to out_dir and returned. Only rewritten when the TTF changes.
def LvglFont(ttf_path, size, name, out_dir, codepoints):
    os.makedirs(out_dir, exist_ok=True)
    source = os.path.join(out_dir, name + ".c")
    if os.path.exists(source) and os.path.getmtime(source) >= os.path.getmtime(ttf_path):
        return source
    from PIL import Image, ImageDraw, ImageFont

    print(f"rendering {name} from {os.path.basename(ttf_path)} at {size} px")
    font = ImageFont.truetype(ttf_path, size)
    ascent, descent = font.getmetrics()
    codepoints = sorted(codepoints)

    # glyphs: id 0 is reserved, then one per code point in order. A glyph's
    # bitmap is its box, row by row, two pixels per byte, high nibble first,
    # padded to a byte at the end; ofs_y is its bottom edge above the baseline.
    glyph_dsc = [
        "    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},"
    ]
    bitmap = bytearray()
    for cp in codepoints:
        ch = chr(cp)
        x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")
        w, h = x1 - x0, y1 - y0
        img = Image.new("L", (max(w, 1), max(h, 1)), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
        pixels = img.getdata() if w and h else []
        nibbles = [v >> 4 for v in pixels]
        if len(nibbles) % 2:
            nibbles.append(0)
        glyph_dsc.append(
            f"    {{.bitmap_index = {len(bitmap)}, .adv_w = {round(font.getlength(ch) * 16)}, "
            f".box_w = {w}, .box_h = {h}, .ofs_x = {x0}, .ofs_y = {-y1}}},"
        )
        bitmap += bytes((a << 4) | b for a, b in zip(nibbles[::2], nibbles[1::2]))
    assert len(bitmap) < (1 << 20), "LVGL fonts hold at most 1 MB of bitmaps"

    # a cmap per run of consecutive code points
    cmaps = []
    run_start = 0
    for i in range(1, len(codepoints) + 1):
        if i == len(codepoints) or codepoints[i] != codepoints[i - 1] + 1:
            cmaps.append(
                f"    {{.range_start = {codepoints[run_start]}, .range_length = {i - run_start}, "
                f".glyph_id_start = {run_start + 1}, .unicode_list = NULL, "
                f".glyph_id_ofs_list = NULL, .list_length = 0, "
                f".type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY}},"
            )
            run_start = i

    rows = [
        "    " + " ".join(f"0x{b:02x}," for b in bitmap[i : i + 16])
        for i in range(0, len(bitmap), 16)
    ]
    with open(source, "w") as f:
        f.write(
            f"""// generated by LvglFont() from {os.path.basename(ttf_path)} at {size} px; do not edit
#include "lvgl.h"

static const uint8_t glyph_bitmap[] = {{
{chr(10).join(rows)}
}};

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {{
{chr(10).join(glyph_dsc)}
}};

static const lv_font_fmt_txt_cmap_t cmaps[] = {{
{chr(10).join(cmaps)}
}};

static const lv_font_fmt_txt_dsc_t font_dsc = {{
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = {len(cmaps)},
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = LV_FONT_FMT_TXT_PLAIN,
}};

// the declaration gives the definition external linkage in C++ too
LV_FONT_DECLARE({name});
const lv_font_t {name} = {{
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,
    .line_height = {ascent + descent},
    .base_line = {descent},
    .subpx = LV_FONT_SUBPX_NONE,
    .underline_position = 0,
    .underline_thickness = 0,
    .dsc = &font_dsc,
}};
"""
        )
    return source
