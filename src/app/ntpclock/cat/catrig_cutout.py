#!/usr/bin/env python3
#
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

"""A calico cat as a 2D cutout puppet.

The cat is one model: a set of parts (body, head, ears, legs, tail) with
pivots, and calico patches painted on the parts. An animation is a function
from frame number to a Pose, the set of joint angles and offsets. Frames are
rendered with Pillow, supersampled for antialiasing.

    catrig.py --preview DIR      GIF and contact strip per animation
    catrig.py --frames DIR       PNG frame sequences per animation
"""

import argparse
import dataclasses
import math
import os

from PIL import Image, ImageDraw

# Supersampling factor for antialiasing.
SS = 3

# Palette. Colors are RGB tuples.
WHITE = (250, 247, 240)
ORANGE = (232, 140, 48)
BLACK = (56, 50, 52)
OUTLINE = (42, 32, 32)
PINK = (240, 150, 160)
EYE_WHITE = (255, 255, 250)
IRIS = (112, 190, 92)
YARN = (210, 60, 80)
YARN_LINE = (150, 30, 50)

# Geometry of the cat, in cat units with the body center at the origin,
# +x toward the nose and +y down. The cat faces right.
BODY_RX, BODY_RY = 72, 38
NECK = (58, -22)
HEAD_CENTER, HEAD_R = (18, -20), 36
FRONT_HIP = (46, 18)
BACK_HIP = (-48, 18)
UPPER_LEN, UPPER_W = 32, 21
LOWER_LEN, LOWER_W = 30, 17
THIGH_LEN, SHIN_LEN, SHIN_W = 26, 26, 15
FOOT_LEN, FOOT_W = 20, 13
TAIL_BASE = (-66, -14)
TAIL_SEGS = ((30, 17), (28, 14), (26, 11))  # (length, width)
GROUND_Y = FRONT_HIP[1] + UPPER_LEN + LOWER_LEN + 10
YARN_R = 20


def dim(color, k=0.82):
    return tuple(int(c * k) for c in color)


# ---------------------------------------------------------------- affine math


def rot(deg):
    r = math.radians(deg)
    c, s = math.cos(r), math.sin(r)
    return (c, -s, 0, s, c, 0)


def trans(x, y):
    return (1, 0, x, 0, 1, y)


def scale(k):
    return (k, 0, 0, 0, k, 0)


def mul(m, n):
    """m after n: apply n first, then m."""
    a, b, c, d, e, f = m
    g, h, i, j, k, l = n
    return (
        a * g + b * j,
        a * h + b * k,
        a * i + b * l + c,
        d * g + e * j,
        d * h + e * k,
        d * i + e * l + f,
    )


def chain(*ms):
    out = ms[0]
    for m in ms[1:]:
        out = mul(out, m)
    return out


def apply(m, pts):
    a, b, c, d, e, f = m
    return [(a * x + b * y + c, d * x + e * y + f) for x, y in pts]


# ---------------------------------------------------------------- shapes


def ellipse(cx, cy, rx, ry, n=64):
    return [
        (cx + rx * math.cos(2 * math.pi * i / n), cy + ry * math.sin(2 * math.pi * i / n))
        for i in range(n)
    ]


def capsule(x0, y0, x1, y1, w, n=16):
    """A line segment with round ends, as a polygon."""
    a = math.atan2(y1 - y0, x1 - x0)
    r = w / 2
    pts = []
    for i in range(n + 1):
        t = a + math.pi / 2 + math.pi * i / n
        pts.append((x0 + r * math.cos(t), y0 + r * math.sin(t)))
    for i in range(n + 1):
        t = a - math.pi / 2 + math.pi * i / n
        pts.append((x1 + r * math.cos(t), y1 + r * math.sin(t)))
    return pts


# ---------------------------------------------------------------- pose


@dataclasses.dataclass
class Pose:
    """All the joints. Angles in degrees. Leg angles: positive swings the foot
    toward the nose; lower-leg angles are relative to the upper leg. Tail
    angles: positive curls toward the back. Legs are named by front/back and
    near/far side (the near side faces the viewer)."""

    body_x: float = 0
    body_y: float = 0
    body_angle: float = 0  # negative lowers the rear
    head_angle: float = 0  # negative tips the nose down
    head_dx: float = 0
    head_dy: float = 0
    ear_near: float = 0
    ear_far: float = 0
    fn_u: float = 4
    fn_l: float = -4
    ff_u: float = -4
    ff_l: float = -4
    bn_u: float = 22
    bn_l: float = -58
    bn_f: float = 66
    bf_u: float = 28
    bf_l: float = -58
    bf_f: float = 60
    tail1: float = 30
    tail2: float = 35
    tail3: float = 35
    eye: float = 1  # 1 open, 0 closed
    pupil_dx: float = 0
    pupil_dy: float = 0
    mouth: float = 0  # 1 fully open
    yarn: float = 0  # 1 shows the yarn ball
    yarn_x: float = 150
    yarn_y: float = GROUND_Y - YARN_R
    yarn_angle: float = 0
    legs_hidden: float = 0  # 1 hides the far legs (sitting, lying)


def lerp(a, b, t):
    return a + (b - a) * t


def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def blend(p, q, t):
    """Pose between p and q, with easing."""
    t = smooth(t)
    return Pose(**{
        f.name: lerp(getattr(p, f.name), getattr(q, f.name), t) for f in dataclasses.fields(Pose)
    })


def keyframes(frames, t):
    """frames: list of (time, Pose) sorted by time, t in [0, 1]."""
    for (t0, p0), (t1, p1) in zip(frames, frames[1:]):
        if t <= t1:
            return blend(p0, p1, (t - t0) / (t1 - t0) if t1 > t0 else 1)
    return frames[-1][1]


def add(p, **deltas):
    """Pose p with deltas added to the named fields."""
    return dataclasses.replace(p, **{k: getattr(p, k) + v for k, v in deltas.items()})


# ---------------------------------------------------------------- the model


class Part:
    """A filled, outlined polygon with color patches clipped to its shape, in
    cat coordinates. Draw order: lower first."""

    def __init__(self, order, shape, color, patches=(), outline=True, width=2.2):
        self.order = order
        self.shape = shape
        self.color = color
        self.patches = list(patches)
        self.outline = outline
        self.width = width


class Stroke:
    """An open polyline, for whiskers and mouths."""

    def __init__(self, order, pts, color=OUTLINE, width=1.6):
        self.order = order
        self.pts = pts
        self.color = color
        self.width = width


def build_leg(parts, B, hip, upper, lower, near, order, sock=None):
    H = chain(B, trans(*hip), rot(-upper))
    K = chain(H, trans(0, UPPER_LEN), rot(-lower))
    base = WHITE if near else dim(WHITE)
    patches = []
    if sock is not None:
        patches.append((apply(H, ellipse(0, 8, 14, 12)), sock if near else dim(sock)))
    parts.append(Part(order, apply(H, capsule(0, 0, 0, UPPER_LEN, UPPER_W)), base, patches))
    parts.append(Part(order + 0.1, apply(K, capsule(0, 0, 0, LOWER_LEN, LOWER_W)), base))
    parts.append(Part(order + 0.2, apply(K, ellipse(5, LOWER_LEN + 1, 14, 9)), base))


def build_hind_leg(parts, B, hip, thigh, shin, foot, near, order, sock=None):
    H = chain(B, trans(*hip), rot(-thigh))
    K = chain(H, trans(0, THIGH_LEN), rot(-shin))
    F = chain(K, trans(0, SHIN_LEN), rot(-foot))
    base = WHITE if near else dim(WHITE)
    patches = []
    if sock is not None:
        patches.append((apply(F, ellipse(FOOT_LEN * 0.6, 0, 16, 12)), sock if near else dim(sock)))
    parts.append(Part(order + 0.1, apply(K, capsule(0, 0, 0, SHIN_LEN, SHIN_W)), base))
    parts.append(Part(order + 0.2, apply(F, capsule(0, 0, 0, FOOT_LEN, FOOT_W)), base, patches))
    parts.append(Part(order + 0.3, apply(F, ellipse(2, FOOT_LEN + 1, 10, 8)), base))
    parts.append(Part(order, apply(H, ellipse(0, THIGH_LEN * 0.45, 17, THIGH_LEN * 0.7)), base))


def build_eye(parts, N, cx, cy, size, pose, order):
    openness = max(pose.eye, 0.0)
    if openness < 0.12:
        parts.append(
            Stroke(order, apply(N, [(cx - size, cy), (cx, cy + 2), (cx + size, cy)]), width=2.0)
        )
        return
    ry = size * 1.3 * openness
    parts.append(Part(order, apply(N, ellipse(cx, cy, size, ry)), EYE_WHITE, width=1.4))
    px, py = cx + pose.pupil_dx, cy + pose.pupil_dy
    parts.append(
        Part(order + 0.1, apply(N, ellipse(px, py, size * 0.72, ry * 0.8)), IRIS, outline=False)
    )
    parts.append(
        Part(order + 0.2, apply(N, ellipse(px, py, size * 0.3, ry * 0.65)), BLACK, outline=False)
    )
    parts.append(
        Part(
            order + 0.3,
            apply(N, ellipse(px - size * 0.3, py - ry * 0.35, size * 0.2, size * 0.2)),
            EYE_WHITE,
            outline=False,
        )
    )


def build(pose):
    """Return the drawable parts of the cat in cat coordinates."""
    parts = []
    B = chain(trans(pose.body_x, pose.body_y), rot(pose.body_angle))

    # far legs, behind everything
    if pose.legs_hidden < 0.5:
        build_hind_leg(parts, B, BACK_HIP, pose.bf_u, pose.bf_l, pose.bf_f, False, 10)
        build_leg(parts, B, FRONT_HIP, pose.ff_u, pose.ff_l, False, 11, sock=ORANGE)

    # tail: three chained segments, orange / white / black
    T = chain(B, trans(*TAIL_BASE))
    for (length, width), angle, color, order in zip(
        TAIL_SEGS, (pose.tail1, pose.tail2, pose.tail3), (ORANGE, WHITE, BLACK), (20, 21, 22)
    ):
        T = mul(T, rot(-angle))
        parts.append(Part(order, apply(T, capsule(0, 0, 0, -length, width)), color))
        T = mul(T, trans(0, -length))

    # body with calico patches
    body_patches = [
        (apply(B, ellipse(14, -26, 42, 24)), ORANGE),
        (apply(B, ellipse(-42, -12, 26, 24)), BLACK),
        (apply(B, ellipse(-8, 18, 16, 12)), BLACK),
    ]
    parts.append(Part(30, apply(B, ellipse(0, 0, BODY_RX, BODY_RY)), WHITE, body_patches))

    # near legs
    if pose.legs_hidden < 0.5:
        build_hind_leg(parts, B, BACK_HIP, pose.bn_u, pose.bn_l, pose.bn_f, True, 40, sock=BLACK)
    build_leg(parts, B, FRONT_HIP, pose.fn_u, pose.fn_l, True, 41)

    # head
    N = chain(B, trans(*NECK), rot(pose.head_angle), trans(pose.head_dx, pose.head_dy))
    hx, hy = HEAD_CENTER
    EF = chain(N, trans(hx - 14, hy - 30), rot(pose.ear_far))
    parts.append(
        Part(
            50,
            apply(EF, [(-6, 4), (-12, -32), (12, 2)]),
            WHITE,
            [(apply(EF, [(-5, 2), (-8, -20), (7, 0)]), PINK)],
        )
    )
    head_patches = [
        (apply(N, ellipse(hx + 20, hy - 10, 24, 22)), ORANGE),
        (apply(N, ellipse(hx - 14, hy - 24, 24, 18)), BLACK),
    ]
    parts.append(Part(51, apply(N, ellipse(hx, hy, HEAD_R, HEAD_R)), WHITE, head_patches))
    EN = chain(N, trans(hx + 16, hy - 29), rot(pose.ear_near))
    parts.append(
        Part(
            52,
            apply(EN, [(-10, 4), (4, -34), (14, 2)]),
            ORANGE,
            [(apply(EN, [(-5, 1), (3, -22), (9, 0)]), PINK)],
        )
    )

    # face
    build_eye(parts, N, hx - 4, hy - 4, 6, pose, 60)
    build_eye(parts, N, hx + 22, hy - 2, 7, pose, 61)
    nose = [(hx + 32, hy + 10), (hx + 42, hy + 10), (hx + 37, hy + 17)]
    parts.append(Part(62, apply(N, nose), PINK, width=1.4))
    mouth_drop = 4 + 10 * pose.mouth
    parts.append(Stroke(63, apply(N, [(hx + 37, hy + 17), (hx + 37, hy + 17 + mouth_drop)])))
    if pose.mouth > 0.2:
        parts.append(
            Part(
                63.5,
                apply(N, ellipse(hx + 37, hy + 20 + mouth_drop, 7 * pose.mouth, 6 * pose.mouth)),
                PINK,
                width=1.4,
            )
        )
    parts.append(
        Stroke(
            64,
            apply(
                N,
                [
                    (hx + 30, hy + 20 + mouth_drop * 0.7),
                    (hx + 37, hy + 17 + mouth_drop),
                    (hx + 44, hy + 20 + mouth_drop * 0.7),
                ],
            ),
        )
    )
    for dy, ey in ((-4, -10), (0, -2), (4, 6)):
        parts.append(
            Stroke(65, apply(N, [(hx + 36, hy + 10 + dy), (hx + 66, hy + 10 + ey)]), width=1.3)
        )

    # yarn ball
    if pose.yarn > 0.5:
        Y = chain(trans(pose.yarn_x, pose.yarn_y), rot(pose.yarn_angle))
        parts.append(Part(5, apply(Y, ellipse(0, 0, YARN_R, YARN_R)), YARN, width=1.8))
        for a in (0, 40, 80, 120):
            parts.append(
                Stroke(
                    6,
                    apply(
                        mul(Y, rot(a)),
                        ellipse(0, 0, YARN_R * 0.95, YARN_R * 0.35, n=40) + [(YARN_R * 0.95, 0)],
                    ),
                    color=YARN_LINE,
                    width=1.3,
                )
            )
    return parts


# ---------------------------------------------------------------- rendering


def render(pose, size, origin, cat_scale=1.0):
    """Render a pose to an RGBA image of the given (w, h). origin is where
    the body center (at rest) lands, in output pixels."""
    w, h = size
    canvas = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    M = chain(scale(SS), trans(*origin), scale(cat_scale))
    for part in sorted(build(pose), key=lambda p: p.order):
        if isinstance(part, Stroke):
            pts = apply(M, part.pts)
            ImageDraw.Draw(canvas).line(
                pts,
                fill=part.color + (255,),
                width=max(1, round(part.width * cat_scale * SS)),
                joint="curve",
            )
            continue
        draw_part(canvas, M, part, cat_scale)
    return canvas.resize((w, h), Image.LANCZOS)


def draw_part(canvas, M, part, cat_scale):
    shape = apply(M, part.shape)
    xs = [p[0] for p in shape]
    ys = [p[1] for p in shape]
    pad = 2 * SS
    x0 = max(0, int(min(xs)) - pad)
    y0 = max(0, int(min(ys)) - pad)
    x1 = min(canvas.width, int(max(xs)) + pad + 1)
    y1 = min(canvas.height, int(max(ys)) + pad + 1)
    if x1 <= x0 or y1 <= y0:
        return
    local = [(x - x0, y - y0) for x, y in shape]

    mask = Image.new("L", (x1 - x0, y1 - y0), 0)
    ImageDraw.Draw(mask).polygon(local, fill=255)
    layer = Image.new("RGBA", mask.size, part.color + (255,))
    d = ImageDraw.Draw(layer)
    for pts, color in part.patches:
        d.polygon([(x - x0, y - y0) for x, y in apply(M, pts)], fill=color + (255,))
    layer.putalpha(mask)
    if part.outline:
        d.line(
            local + local[:2],
            fill=OUTLINE + (255,),
            width=max(1, round(part.width * cat_scale * SS)),
            joint="curve",
        )
    canvas.alpha_composite(layer, dest=(x0, y0))


# ---------------------------------------------------------------- animations


@dataclasses.dataclass
class Anim:
    name: str
    frames: int
    fps: int
    pose: object  # frame index -> Pose
    loop: bool = True
    # horizontal speed in cat units per frame, for the scene preview
    speed: float = 0


def tau(i, n):
    return 2 * math.pi * i / n


REST = Pose()


def walk_pose(i, n=24):
    phi = tau(i, n)
    amp = 24
    p = Pose()
    for name, phase in (("bf", 0.0), ("ff", 0.25), ("bn", 0.5), ("fn", 0.75)):
        a = phi + 2 * math.pi * phase
        lift = max(0.0, math.cos(a))
        if name[0] == "f":
            setattr(p, name + "_u", amp * math.sin(a))
            setattr(p, name + "_l", -4 - 45 * lift)
        else:
            u = 25 + amp * math.sin(a)
            l = -58 - 35 * lift
            setattr(p, name + "_u", u)
            setattr(p, name + "_l", l)
            setattr(p, name + "_f", 32 - (u + l) + 25 * lift)
    p.body_y = 2.5 * math.sin(2 * phi)
    p.head_dy = -2 * math.sin(2 * phi)
    p.head_angle = 3 * math.sin(2 * phi + 1)
    p.tail1 += 6 * math.sin(phi)
    p.tail2 += 8 * math.sin(phi - 0.8)
    p.tail3 += 10 * math.sin(phi - 1.6)
    p.ear_near = 2 * math.sin(2 * phi)
    return p


def run_pose(i, n=16):
    phi = tau(i, n)
    p = Pose()
    amp = 42
    for name, a in (
        ("fn", phi),
        ("ff", phi - 0.5),
        ("bn", phi + math.pi),
        ("bf", phi + math.pi - 0.5),
    ):
        lift = max(0.0, math.cos(a))
        if name[0] == "f":
            setattr(p, name + "_u", amp * math.sin(a))
            setattr(p, name + "_l", -4 - 60 * lift)
        else:
            u = 25 + amp * math.sin(a)
            l = -58 - 50 * lift
            setattr(p, name + "_u", u)
            setattr(p, name + "_l", l)
            setattr(p, name + "_f", 32 - (u + l) + 30 * lift)
    p.body_y = -9 * max(0.0, math.sin(phi + 0.6)) + 2
    p.body_angle = 7 * math.sin(phi + 0.6)
    p.head_angle = -p.body_angle * 0.7
    p.tail1, p.tail2, p.tail3 = (
        75 + 8 * math.sin(phi),
        8 + 10 * math.sin(phi - 1),
        5 + 12 * math.sin(phi - 2),
    )
    p.ear_near = p.ear_far = -8
    p.mouth = 0.25
    return p


# The classic sitting pose: rear on the ground, back sloping up to the head,
# front legs straight, hind legs folded under the haunch.
SIT = Pose(
    body_x=-6,
    body_y=20,
    body_angle=-32,
    head_angle=24,
    head_dx=-4,
    head_dy=-6,
    fn_u=-30,
    fn_l=0,
    ff_u=-35,
    ff_l=0,
    bn_u=18,
    bn_l=-110,
    bn_f=150,
    bf_u=14,
    bf_l=-110,
    bf_f=150,
    tail1=125,
    tail2=60,
    tail3=55,
)

# Sitting with the near front paw lifted to the mouth.
LICK = dataclasses.replace(SIT, fn_u=55, fn_l=-150, head_angle=-14, head_dx=-10, head_dy=6, eye=0.3)


def sit_groom_pose(i, n=96):
    t = i / n
    p = keyframes(
        [
            (0.00, REST),
            (0.12, SIT),
            (0.22, SIT),
            (0.30, LICK),
            (0.64, LICK),
            (0.72, SIT),
            (1.00, SIT),
        ],
        t,
    )
    if 0.30 <= t <= 0.64:
        lick = math.sin(2 * math.pi * (t - 0.30) * 10)
        p.head_angle += 7 * lick
        p.head_dy += 2 * lick
        p.fn_u += 3 * lick
    elif t > 0.80:
        # look at the viewer, blink once
        p.pupil_dx = -1.5
        p.pupil_dy = 1
        blink = (t - 0.90) / 0.06
        if 0 <= blink <= 1:
            p.eye = 1 - math.sin(math.pi * blink)
    if t > 0.22:
        p.tail1 += 6 * math.sin(2 * math.pi * t * 1.5)
        p.tail3 += 12 * math.sin(2 * math.pi * t * 1.5 - 1)
    return p


def chase_pose(i, n=16):
    p = run_pose(i, n)
    p.yarn = 1
    p.yarn_x = 160
    p.yarn_y = GROUND_Y - YARN_R - 6 * abs(math.sin(tau(i, n) * 1.5))
    p.yarn_angle = -360 * i / n * 1.5
    return p


ANIMS = [
    Anim("walk", 24, 24, walk_pose, speed=5.6),
    Anim("run", 16, 24, run_pose, speed=12),
    Anim("sit_groom", 96, 16, sit_groom_pose, loop=False),
    Anim("chase", 16, 24, chase_pose, speed=12),
]

# Output frame geometry, in output pixels at cat_scale 1.
FRAME_SIZE = (320, 224)
ORIGIN = (120, 108)


# ---------------------------------------------------------------- outputs


def frames_of(anim, cat_scale=1.0):
    w, h = FRAME_SIZE
    size = (int(w * cat_scale), int(h * cat_scale))
    origin = (ORIGIN[0] * cat_scale, ORIGIN[1] * cat_scale)
    return [render(anim.pose(i), size, origin, cat_scale) for i in range(anim.frames)]


def on_background(img, color=(120, 170, 220, 255)):
    bg = Image.new("RGBA", img.size, color)
    bg.alpha_composite(img)
    return bg


def write_gif(path, frames, fps, loop=True):
    frames = [on_background(f).convert("RGB") for f in frames]
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=int(1000 / fps),
        loop=0 if loop else 1,
        optimize=False,
    )


def write_strip(path, frames, count=8):
    step = max(1, len(frames) // count)
    picks = frames[::step][:count]
    w, h = picks[0].size
    strip = Image.new("RGBA", (w * len(picks), h), (120, 170, 220, 255))
    for k, f in enumerate(picks):
        strip.alpha_composite(f, dest=(k * w, 0))
    strip.convert("RGB").save(path)


def write_scene(path, anim, frames, width=800, height=480):
    """The animation translating across a screen-sized canvas."""
    out = []
    n = len(frames)
    total = int((width + FRAME_SIZE[0]) / anim.speed) if anim.speed else n * 2
    for k in range(total):
        bg = Image.new("RGBA", (width, height), (30, 34, 60, 255))
        x = int(-FRAME_SIZE[0] + k * anim.speed) if anim.speed else (width - FRAME_SIZE[0]) // 2
        bg.alpha_composite(frames[k % n], dest=(x, height - FRAME_SIZE[1] - 20))
        out.append(bg.convert("RGB"))
    out[0].save(path, save_all=True, append_images=out[1:], duration=int(1000 / anim.fps), loop=0)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--preview", metavar="DIR", help="write GIFs and contact strips")
    ap.add_argument("--frames", metavar="DIR", help="write PNG frame sequences")
    ap.add_argument("--scale", type=float, default=1.0, help="cat scale factor")
    ap.add_argument("--only", action="append", help="limit to these animations")
    args = ap.parse_args()

    for anim in ANIMS:
        if args.only and anim.name not in args.only:
            continue
        frames = frames_of(anim, args.scale)
        if args.preview:
            os.makedirs(args.preview, exist_ok=True)
            write_gif(os.path.join(args.preview, anim.name + ".gif"), frames, anim.fps, anim.loop)
            write_strip(os.path.join(args.preview, anim.name + "_strip.png"), frames)
            write_scene(os.path.join(args.preview, anim.name + "_scene.gif"), anim, frames)
        if args.frames:
            d = os.path.join(args.frames, anim.name)
            os.makedirs(d, exist_ok=True)
            for i, f in enumerate(frames):
                f.save(os.path.join(d, f"{i:03d}.png"))
        print(f"{anim.name}: {anim.frames} frames @ {anim.fps} fps")


if __name__ == "__main__":
    main()
