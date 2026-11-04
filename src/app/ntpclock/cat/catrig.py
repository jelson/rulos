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

"""A calico cat as a 2D puppet.

The cat is one model: a set of parts (body, head, ears, legs, tail) with
pivots, and calico patches painted on the parts. An animation is a function
from frame number to a Pose, the set of joint angles and offsets. Frames are
rendered with Pillow, supersampled for antialiasing.

Parts are drawn in groups. A group's shapes are outlined as one silhouette,
so a leg made of three segments has no lines at its joints, and the head,
muzzle and ears read as one head.

    catrig.py --preview DIR      GIF and contact strip per show
    catrig.py --export DIR       PNG frames and a manifest for the clock build
"""

import argparse
import dataclasses
import json
import math
import os

from PIL import Image, ImageChops, ImageDraw

# Supersampling factor for antialiasing.
SS = 3

# Palette. Colors are RGB, or RGBA for translucent shading.
WHITE = (252, 248, 240)
CREAM = (250, 236, 212)
ORANGE = (236, 146, 52)
STRIPE = (205, 110, 30)
BLACK = (58, 52, 56)
OUTLINE = (44, 34, 36)
PINK = (242, 150, 160)
NOSE = (232, 120, 135)
EYE_WHITE = (255, 255, 250)
IRIS = (228, 182, 62)
IRIS_DARK = (156, 112, 30)
YARN = (210, 60, 80)
YARN_LINE = (150, 30, 50)
COLLAR = (200, 44, 64)
BELL = (242, 196, 70)
SHADE = (40, 20, 30, 42)
LIGHT = (255, 255, 255, 60)

# Geometry of the cat, in cat units with the body center at the origin,
# +x toward the nose and +y down. The cat faces right.
BODY_RX, BODY_RY = 74, 35
NECK = (58, -20)
HEAD_CENTER, HEAD_RX, HEAD_RY = (14, -22), 38, 34
FRONT_HIP = (46, 16)
BACK_HIP = (-48, 16)
UPPER_LEN, UPPER_W = 32, 16
LOWER_LEN, LOWER_W = 30, 13
THIGH_LEN, SHIN_LEN, SHIN_W = 26, 26, 12
FOOT_LEN, FOOT_W = 20, 11
TAIL_BASE = (-68, -12)
TAIL_SEGS = ((34, 13), (32, 11), (30, 9))  # (length, width)
GROUND_Y = FRONT_HIP[1] + UPPER_LEN + LOWER_LEN + 12
YARN_R = 20


def dim(color, k=0.84):
    return tuple(int(c * k) for c in color[:3]) + tuple(color[3:])


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


def egg(cx, cy, rx, ry, taper=0.12, n=64):
    """An ellipse that is fatter at -x (the rear) than at +x."""
    pts = []
    for i in range(n):
        t = 2 * math.pi * i / n
        k = 1 - taper * math.cos(t)
        pts.append((cx + rx * math.cos(t), cy + ry * k * math.sin(t)))
    return pts


def tufted_ellipse(cx, cy, rx, ry, tufts, n=96):
    """An ellipse with fur spikes. tufts: list of (angle_deg, length, half_width_deg)."""
    pts = []
    for i in range(n):
        a = 360 * i / n
        k = 1
        for center, length, half in tufts:
            d = abs((a - center + 180) % 360 - 180)
            if d < half:
                k = max(k, 1 + length / ry * (1 - d / half))
        t = math.radians(a)
        pts.append((cx + rx * k * math.cos(t), cy + ry * k * math.sin(t)))
    return pts


def capsule(x0, y0, x1, y1, w, n=16, tuft=0):
    """A line segment with round ends, as a polygon. tuft > 0 turns the far
    end into fur spikes of that length."""
    a = math.atan2(y1 - y0, x1 - x0)
    r = w / 2
    pts = []
    for i in range(n + 1):
        t = a + math.pi / 2 + math.pi * i / n
        pts.append((x0 + r * math.cos(t), y0 + r * math.sin(t)))
    for i in range(n + 1):
        t = a - math.pi / 2 + math.pi * i / n
        rr = r
        if tuft:
            # three spikes across the cap
            phase = (i / n) * 3 * math.pi
            rr = r + tuft * abs(math.sin(phase))
        pts.append((x1 + rr * math.cos(t), y1 + rr * math.sin(t)))
    return pts


def tube(joints, widths, per_seg=10, tuft=0):
    """A smooth tapered tube along a Catmull-Rom spline through the joints,
    as a list of polygons to be unioned: a disc at every sample and a quad
    between neighbors. A union cannot fold over itself the way an offset
    outline does at a sharp bend. widths gives the diameter at each joint.
    tuft > 0 adds fur spikes of that length to the end cap."""
    P = [joints[0]] + list(joints) + [joints[-1]]
    center, width = [], []
    for i in range(1, len(P) - 2):
        (x0, y0), (x1, y1), (x2, y2), (x3, y3) = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(per_seg):
            t = k / per_seg
            t2, t3 = t * t, t * t * t
            center.append((
                0.5
                * (
                    2 * x1
                    + (-x0 + x2) * t
                    + (2 * x0 - 5 * x1 + 4 * x2 - x3) * t2
                    + (-x0 + 3 * x1 - 3 * x2 + x3) * t3
                ),
                0.5
                * (
                    2 * y1
                    + (-y0 + y2) * t
                    + (2 * y0 - 5 * y1 + 4 * y2 - y3) * t2
                    + (-y0 + 3 * y1 - 3 * y2 + y3) * t3
                ),
            ))
            width.append(lerp(widths[i - 1], widths[i], t))
    center.append(joints[-1])
    width.append(widths[-1])

    polys = []
    for i, (x, y) in enumerate(center):
        polys.append(ellipse(x, y, width[i] / 2, width[i] / 2, n=24))
        if i + 1 < len(center):
            xb, yb = center[i + 1]
            dx, dy = xb - x, yb - y
            norm = math.hypot(dx, dy) or 1
            nx, ny = -dy / norm, dx / norm
            ra, rb = width[i] / 2, width[i + 1] / 2
            polys.append([
                (x + nx * ra, y + ny * ra),
                (xb + nx * rb, yb + ny * rb),
                (xb - nx * rb, yb - ny * rb),
                (x - nx * ra, y - ny * ra),
            ])
    if tuft:
        x, y = center[-1]
        xa, ya = center[-2]
        a = math.atan2(y - ya, x - xa)
        r = width[-1] / 2
        polys.append(
            [
                (
                    x
                    + (r + tuft * abs(math.sin(3 * math.pi * i / 10)))
                    * math.cos(a + math.pi / 2 - math.pi * i / 10),
                    y
                    + (r + tuft * abs(math.sin(3 * math.pi * i / 10)))
                    * math.sin(a + math.pi / 2 - math.pi * i / 10),
                )
                for i in range(11)
            ]
            + [(x, y)]
        )
    return polys


def almond(cx, cy, r, up, down, n=14):
    pts = []
    for i in range(n + 1):
        t = math.pi - math.pi * i / n
        pts.append((cx + r * math.cos(t), cy - up * math.sin(t)))
    for i in range(1, n):
        t = math.pi * i / n
        pts.append((cx + r * math.cos(t), cy + down * math.sin(t)))
    return pts


# ---------------------------------------------------------------- pose


@dataclasses.dataclass
class Pose:
    """All the joints. Angles in degrees. Leg angles: positive swings the foot
    toward the nose; lower-leg and foot angles are relative to the segment
    above. Tail angles: positive curls toward the back. Legs are named by
    front/back and near/far side (the near side faces the viewer)."""

    body_x: float = 0
    body_y: float = 0
    body_angle: float = 0  # negative lowers the rear
    head_angle: float = 0  # positive tips the nose down
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
    pupil: float = 1  # pupil width, 1 normal, 2 dilated
    mouth: float = 0  # 1 fully open
    tongue: float = 0  # 1 tongue out
    yarn: float = 0  # 1 shows the yarn ball
    yarn_x: float = 150
    yarn_y: float = GROUND_Y - YARN_R
    yarn_angle: float = 0
    legs_hidden: float = 0  # 1 hides the far legs (sitting, lying)
    haunch: float = 0  # 1 swells the hind thighs into a sitting haunch
    happy: float = 0  # 1 draws closed eyes as happy arcs
    puff: float = 1  # tail thickness multiplier
    zzz: float = -1  # >= 0 shows sleeping z's, advancing with the value
    bfly: float = 0  # 1 shows the butterfly
    bfly_x: float = 120
    bfly_y: float = -60
    bfly_wing: float = 0  # wing flap phase, in cycles


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


class Group:
    """Shapes drawn as one outlined silhouette, with color patches clipped to
    a shape or to another patch, and strokes drawn on top. Coordinates are
    cat units. Groups draw in order of `order`, lowest first."""

    def __init__(self, order, width=2.4):
        self.order = order
        self.width = width
        self.shapes = []  # (polygon, color)
        self.patches = []  # (polygon, color, clip) with clip ("s", i) or ("p", i)
        self.strokes = []  # (points, color, width)

    def shape(self, poly, color):
        """poly: a polygon (list of points) or a list of polygons that are
        unioned into one shape."""
        if isinstance(poly[0][0], (int, float)):
            poly = [poly]
        self.shapes.append((poly, color))
        return ("s", len(self.shapes) - 1)

    def patch(self, poly, color, clip):
        self.patches.append((poly, color, clip))
        return ("p", len(self.patches) - 1)

    def stroke(self, pts, color=OUTLINE, width=1.5):
        self.strokes.append((pts, color, width))

    def points(self):
        for polys, _ in self.shapes:
            for poly in polys:
                yield from poly
        for pts, _, _ in self.strokes:
            yield from pts


def build_front_leg(groups, B, hip, upper, lower, near, order, sock=None):
    H = chain(B, trans(*hip), rot(-upper))
    K = chain(H, trans(0, UPPER_LEN), rot(-lower))
    base = WHITE if near else dim(WHITE)
    g = Group(order)
    shoulder, elbow = apply(H, [(0, -6), (0, UPPER_LEN)])
    wrist, toe = apply(K, [(0, LOWER_LEN - 4), (6, LOWER_LEN + 4)])
    s_leg = g.shape(
        tube([shoulder, elbow, wrist, toe], [UPPER_W + 2, UPPER_W - 4, LOWER_W, LOWER_W + 2]), base
    )
    if sock is not None:
        g.patch(apply(H, ellipse(0, 6, 12, 14)), sock if near else dim(sock), s_leg)
    for dx in (2, 7):
        g.stroke(apply(K, [(dx, LOWER_LEN + 6), (dx, LOWER_LEN + 10)]), width=1.2)
    groups.append(g)


def build_hind_leg(groups, B, hip, thigh, shin, foot, near, order, haunch=0, sock=None):
    H = chain(B, trans(*hip), rot(-thigh))
    K = chain(H, trans(0, THIGH_LEN), rot(-shin))
    F = chain(K, trans(0, SHIN_LEN), rot(-foot))
    base = WHITE if near else dim(WHITE)
    g = Group(order)
    g.shape(
        apply(
            H,
            ellipse(
                0,
                THIGH_LEN * lerp(0.4, 0.55, haunch),
                lerp(15, 23, haunch),
                THIGH_LEN * lerp(0.7, 1.0, haunch),
            ),
        ),
        base,
    )
    hip, knee = apply(H, [(0, 0), (0, THIGH_LEN)])
    hock = apply(K, [(0, SHIN_LEN)])[0]
    toe = apply(F, [(3, FOOT_LEN + 3)])[0]
    s_leg = g.shape(tube([hip, knee, hock, toe], [24, SHIN_W + 2, FOOT_W - 1, FOOT_W + 2]), base)
    if sock is not None:
        g.patch(apply(F, ellipse(FOOT_LEN * 0.65, 0, 15, 12)), sock if near else dim(sock), s_leg)
    for dx in (0, 5):
        g.stroke(apply(F, [(dx, FOOT_LEN + 5), (dx, FOOT_LEN + 9)]), width=1.2)
    groups.append(g)


def build_eye(groups, N, cx, cy, r, pose, order):
    openness = max(pose.eye, 0.0)
    g = Group(order, width=1.5)
    if openness < 0.12:
        if pose.happy > 0.5:
            arc = [
                (cx + r * math.cos(t), cy + 2 - r * 0.8 * math.sin(t))
                for t in (math.pi * i / 8 for i in range(9))
            ]
        else:
            arc = [(cx - r, cy), (cx, cy + 3), (cx + r, cy)]
        g.stroke(apply(N, arc), width=2.0)
        groups.append(g)
        return
    up = r * 1.15 * openness
    down = r * 0.75 * max(openness, 0.4)
    eye = g.shape(apply(N, almond(cx, cy, r, up, down)), EYE_WHITE)
    px, py = cx + pose.pupil_dx, cy + pose.pupil_dy
    iris = g.patch(apply(N, ellipse(px, py + 1, r * 0.9, r * 1.0)), IRIS_DARK, eye)
    g.patch(apply(N, ellipse(px, py + 1, r * 0.72, r * 0.82)), IRIS, iris)
    g.patch(apply(N, ellipse(px, py + 1, r * 0.22 * pose.pupil, r * 0.95)), BLACK, iris)
    g.patch(apply(N, ellipse(px - r * 0.3, py - r * 0.35, r * 0.2, r * 0.22)), EYE_WHITE, eye)
    # upper lid
    lid = [
        (cx + r * math.cos(t), cy - up * math.sin(t)) for t in (math.pi * i / 10 for i in range(11))
    ]
    g.stroke(apply(N, lid), width=2.2)
    groups.append(g)


def build(pose):
    """Return the drawable groups of the cat in cat coordinates."""
    groups = []
    B = chain(trans(pose.body_x, pose.body_y), rot(pose.body_angle))

    # far legs, behind everything
    if pose.legs_hidden < 0.5:
        build_hind_leg(
            groups, B, BACK_HIP, pose.bf_u, pose.bf_l, pose.bf_f, False, 10, haunch=pose.haunch
        )
        build_front_leg(groups, B, FRONT_HIP, pose.ff_u, pose.ff_l, False, 11)

    # tail: three chained segments, orange / white / black, fluffy tip
    tail = Group(20)
    T = chain(B, trans(*TAIL_BASE))
    joints = [apply(T, [(0, 0)])[0]]
    widths = [(TAIL_SEGS[0][1] + 2) * pose.puff]
    for (length, width), angle in zip(TAIL_SEGS, (pose.tail1, pose.tail2, pose.tail3)):
        T = chain(T, rot(-angle), trans(0, -length))
        joints.append(apply(T, [(0, 0)])[0])
        widths.append(width * pose.puff)
    s_tail = tail.shape(tube(joints, widths, tuft=3 * pose.puff), BLACK)
    # the orange band around the middle of the tail
    jx, jy = joints[1]
    kx, ky = joints[2]
    tail.patch(ellipse((jx + kx) / 2, (jy + ky) / 2, 20, 20), ORANGE, s_tail)
    groups.append(tail)

    # body, with the neck joining it to the head
    body = Group(30)
    s_body = body.shape(apply(B, egg(0, 0, BODY_RX, BODY_RY)), WHITE)
    hx, hy = HEAD_CENTER
    N = chain(B, trans(*NECK), rot(pose.head_angle), trans(pose.head_dx, pose.head_dy))
    head_world = apply(N, [HEAD_CENTER])[0]
    neck_world = apply(B, [(46, -6)])[0]
    s_neck = body.shape(
        capsule(neck_world[0], neck_world[1], head_world[0], head_world[1], 40), WHITE
    )
    # tortoiseshell saddle: black over the back with big orange patches
    saddle = body.patch(apply(B, ellipse(-16, -18, 60, 27)), BLACK, s_body)
    body.patch(apply(B, ellipse(-2, -22, 17, 25)), ORANGE, saddle)
    body.patch(apply(B, ellipse(-52, -4, 14, 19)), ORANGE, saddle)
    body.patch(apply(B, ellipse(30, -34, 10, 9)), ORANGE, saddle)
    body.patch(apply(B, ellipse(0, 33, 70, 16)), SHADE, s_body)
    body.patch(apply(B, ellipse(-10, -40, 50, 12)), LIGHT, s_body)
    groups.append(body)

    # near legs
    if pose.legs_hidden < 0.5:
        build_hind_leg(
            groups, B, BACK_HIP, pose.bn_u, pose.bn_l, pose.bn_f, True, 40, haunch=pose.haunch
        )
    build_front_leg(groups, B, FRONT_HIP, pose.fn_u, pose.fn_l, True, 41)

    # collar: a band across the throat just below the head, with a bell
    ang = math.atan2(head_world[1] - neck_world[1], head_world[0] - neck_world[0])
    C = chain(trans(*head_world), rot(math.degrees(ang)), trans(-HEAD_RY - 6, 0))
    collar = Group(45, width=1.6)
    collar.shape(apply(C, [(-5, -24), (5, -24), (5, 24), (-5, 24)]), COLLAR)
    bell = Group(46, width=1.6)
    bx, by = apply(C, [(6, 22)])[0]
    bell.shape(ellipse(bx, by, 7, 7), BELL)
    bell.stroke([(bx - 4, by + 1), (bx + 4, by + 1)], width=1.2)
    bell.stroke([(bx, by + 2), (bx, by + 5)], width=1.4)
    groups.extend([collar, bell])

    # head: a wide skull with cheek tufts, two whisker pads and a chin, and
    # big ears set wide apart, all one silhouette. The face is seen from
    # three-quarters: the nose sits between the eyes.
    head = Group(50)
    EF = chain(N, trans(hx - 17, hy - 27), rot(pose.ear_far))
    EN = chain(N, trans(hx + 19, hy - 27), rot(pose.ear_near))
    s_ear_far = head.shape(apply(EF, [(-10, 8), (-15, -14), (-9, -36), (6, -14), (12, 6)]), ORANGE)
    s_head = head.shape(
        apply(
            N,
            tufted_ellipse(
                hx,
                hy,
                HEAD_RX,
                HEAD_RY,
                [(145, 8, 10), (163, 10, 10), (181, 7, 9), (18, 6, 9), (36, 7, 9)],
            ),
        ),
        WHITE,
    )
    mx, my = hx + 13, hy + 7  # the nose
    s_pad_far = head.shape(apply(N, ellipse(mx - 10, my + 8, 13, 9)), WHITE)
    s_pad_near = head.shape(apply(N, ellipse(mx + 11, my + 8, 14, 10)), WHITE)
    s_chin = head.shape(apply(N, ellipse(mx + 1, my + 18, 10, 7)), WHITE)
    s_ear_near = head.shape(apply(EN, [(-13, 8), (-9, -18), (2, -40), (13, -16), (15, 8)]), ORANGE)
    head.patch(apply(EF, [(-6, 4), (-7, -22), (5, -2)]), PINK, s_ear_far)
    head.patch(apply(EN, [(-7, 3), (2, -26), (9, 0)]), PINK, s_ear_near)
    head.patch(apply(N, ellipse(hx + 2, hy - 30, 30, 14)), ORANGE, s_head)
    head.patch(apply(N, ellipse(hx + 6, hy - 10, 42, 21)), BLACK, s_head)
    head.patch(
        apply(N, [(mx - 3, hy - 32), (mx + 3, hy - 32), (mx + 11, my + 4), (mx - 11, my + 4)]),
        WHITE,
        s_head,
    )
    for sh in (s_pad_far, s_pad_near, s_chin):
        head.patch(apply(N, ellipse(mx, my + 10, 40, 20)), CREAM, sh)
    head.patch(apply(N, ellipse(mx, my + 2, 22, 12)), CREAM, s_head)
    head.patch(apply(N, ellipse(hx, hy + 30, 36, 12)), SHADE, s_head)
    # mouth: a short line down from the nose, then a curve to each side
    drop = 3 + 8 * pose.mouth
    head.stroke(apply(N, [(mx, my + 2), (mx, my + 3 + drop)]), width=1.4)
    for side in (-1, 1):
        head.stroke(
            apply(
                N,
                [
                    (mx, my + 3 + drop),
                    (mx + side * 4, my + 6 + drop),
                    (mx + side * 9, my + 4 + drop),
                ],
            ),
            width=1.4,
        )
    # whisker dots and whiskers on both pads
    for dx, dy in ((-11, 5), (-8, 10), (-14, 10), (10, 5), (7, 10), (14, 10)):
        head.stroke(apply(N, [(mx + dx, my + dy), (mx + dx + 0.5, my + dy)]), width=1.8)
    for dy, ey in ((2, -10), (6, -2), (10, 6)):
        head.stroke(apply(N, [(mx + 16, my + dy), (mx + 44, my + ey)]), width=1.2)
        head.stroke(apply(N, [(mx - 15, my + dy), (mx - 42, my + ey - 2)]), width=1.2)
    groups.append(head)

    if pose.tongue > 0.1:
        tongue = Group(49.5, width=1.2)
        tongue.shape(
            apply(N, ellipse(mx, my + 8 + drop + 5 * pose.tongue, 4, 6 * pose.tongue)), NOSE
        )
        groups.append(tongue)

    nose = Group(55, width=1.3)
    nose.shape(
        apply(N, [(mx - 6, my - 4), (mx + 6, my - 4), (mx + 1, my + 2), (mx - 1, my + 2)]), NOSE
    )
    groups.append(nose)

    build_eye(groups, N, mx - 17, hy - 6, 6.5, pose, 60)
    build_eye(groups, N, mx + 17, hy - 6, 7.5, pose, 61)

    # yarn ball
    if pose.yarn > 0.5:
        Y = chain(trans(pose.yarn_x, pose.yarn_y), rot(pose.yarn_angle))
        yarn = Group(5, width=1.8)
        ball = yarn.shape(apply(Y, ellipse(0, 0, YARN_R, YARN_R)), YARN)
        yarn.patch(apply(Y, ellipse(4, 6, YARN_R * 0.9, YARN_R * 0.8)), SHADE, ball)
        for a in (0, 40, 80, 120):
            yarn.stroke(
                apply(
                    mul(Y, rot(a)),
                    ellipse(0, 0, YARN_R * 0.95, YARN_R * 0.35, n=40) + [(YARN_R * 0.95, 0)],
                ),
                color=YARN_LINE,
                width=1.3,
            )
        groups.append(yarn)

    # sleeping z's drifting up from the head
    if pose.zzz >= 0:
        zs = Group(70, width=0)
        hx0, hy0 = head_world
        for k in range(3):
            t = (pose.zzz + k / 3) % 1
            if t > 0.85:
                continue
            size = 5 + 9 * t
            x, y = hx0 + 30 + 22 * t, hy0 - 45 - 50 * t
            zs.stroke([(x, y), (x + size, y), (x, y + size), (x + size, y + size)], width=1.8)
        groups.append(zs)

    # a butterfly: two wings flapping about a small body
    if pose.bfly > 0.5:
        flap = abs(math.cos(2 * math.pi * pose.bfly_wing))
        W = trans(pose.bfly_x, pose.bfly_y)
        bf = Group(72, width=1.2)
        for side in (-1, 1):
            wing = bf.shape(apply(W, ellipse(side * 11 * flap, -3, 11 * flap, 9)), (250, 200, 60))
            bf.patch(apply(W, ellipse(side * 12 * flap, -4, 5 * flap, 4)), (230, 120, 40), wing)
        bf.shape(apply(W, capsule(0, -9, 0, 8, 4)), BLACK)
        for side in (-1, 1):
            bf.stroke(apply(W, [(0, -9), (side * 5, -16)]), width=1.0)
        groups.append(bf)
    return groups


# ---------------------------------------------------------------- rendering


def render(pose, size, origin, cat_scale=1.0):
    """Render a pose to an RGBA image of the given (w, h). origin is where
    the body center (at rest) lands, in output pixels."""
    w, h = size
    canvas = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    M = chain(scale(SS), trans(*origin), scale(cat_scale))
    for group in sorted(build(pose), key=lambda g: g.order):
        draw_group(canvas, M, group, cat_scale * SS)
    return canvas.resize((w, h), Image.LANCZOS)


def _poly_mask(size, polys):
    m = Image.new("L", size, 0)
    d = ImageDraw.Draw(m)
    for poly in polys:
        d.polygon(poly, fill=255)
    return m


def draw_group(canvas, M, group, px):
    """px: output pixels per cat unit, including supersampling."""
    pts = apply(M, list(group.points()))
    if not pts:
        return
    pad = int(4 * px)
    x0 = max(0, int(min(p[0] for p in pts)) - pad)
    y0 = max(0, int(min(p[1] for p in pts)) - pad)
    x1 = min(canvas.width, int(max(p[0] for p in pts)) + pad + 1)
    y1 = min(canvas.height, int(max(p[1] for p in pts)) + pad + 1)
    if x1 <= x0 or y1 <= y0:
        return
    L = mul(trans(-x0, -y0), M)
    size = (x1 - x0, y1 - y0)
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # the silhouette outline: every shape's edge, stroked thick, under all
    # the fills so only the outer edge of the union survives
    shapes = [([apply(L, poly) for poly in polys], color) for polys, color in group.shapes]
    if group.width:
        for polys, _ in shapes:
            for poly in polys:
                d.line(
                    poly + poly[:2],
                    fill=OUTLINE + (255,),
                    width=max(1, round(2 * group.width * px)),
                    joint="curve",
                )
    for polys, color in shapes:
        for poly in polys:
            d.polygon(poly, fill=color[:3] + (255,))

    masks = {}
    for i, (polys, _) in enumerate(shapes):
        masks[("s", i)] = _poly_mask(size, polys)
    for j, (poly, color, clip) in enumerate(group.patches):
        m = ImageChops.multiply(_poly_mask(size, [apply(L, poly)]), masks[clip])
        masks[("p", j)] = m
        if len(color) == 4:
            tint = Image.new("RGBA", size, color[:3] + (255,))
            tint.putalpha(m.point(lambda v, a=color[3]: v * a // 255))
            layer.alpha_composite(tint)
        else:
            layer.paste(color, mask=m)

    for pts, color, width in group.strokes:
        d.line(apply(L, pts), fill=color + (255,), width=max(1, round(width * px)), joint="curve")
    canvas.alpha_composite(layer, dest=(x0, y0))


# ---------------------------------------------------------------- animations
#
# A show is a Pose for every frame plus a path: where the cat's body origin
# sits on the screen each frame, in cat units from the screen's left edge
# and up from the ground line, and whether she is mirrored to face left.
# Shows are built from segments played back to back: each segment has its
# own pose function, a horizontal speed, and optionally an arc (a jump).
# Walk and run cycles repeat within a segment, so frames are keyed to find
# the unique poses and render each only once.

SCREEN_W = 800
SCREEN_H = 480
GROUND_LINE = 440  # screen y of the feet
OFF_LEFT = -200  # body origin x when she is fully off the left edge
OFF_RIGHT = SCREEN_W + 200

WALK_SPEED = 5.6  # cat units per frame at 24 fps, matched to the stride
RUN_SPEED = 12
FPS = 24


@dataclasses.dataclass
class Seg:
    n: int
    pose: object  # local frame -> Pose
    speed: float = 0  # body origin dx per frame
    cycle: int = 0  # the pose repeats every `cycle` frames
    arc: object = None  # local frame -> (dx, dy) on top of the linear motion
    flip: bool = False


@dataclasses.dataclass
class Anim:
    name: str
    start_x: float
    segs: list
    fps: int = FPS

    @property
    def length(self):
        return sum(seg.n for seg in self.segs)

    def _locate(self, i):
        x = self.start_x
        for k, seg in enumerate(self.segs):
            if i < seg.n:
                return k, seg, i, x
            i -= seg.n
            x += seg.speed * seg.n
            if seg.arc:
                x += seg.arc(seg.n)[0]
        raise IndexError(i)

    def pose(self, i):
        _, seg, local, _ = self._locate(i)
        return seg.pose(local % seg.cycle if seg.cycle else local)

    def key(self, i):
        """Frames with equal keys have equal poses."""
        k, seg, local, _ = self._locate(i)
        return (k, local % seg.cycle if seg.cycle else local)

    def path(self, i):
        _, seg, local, x = self._locate(i)
        x += seg.speed * local
        y = 0
        if seg.arc:
            dx, dy = seg.arc(local)
            x, y = x + dx, y + dy
        return x, y, seg.flip

    def sample(self, fps):
        """The timeline frames to show when playing at fps."""
        count = math.ceil(self.length * fps / self.fps)
        return [min(round(i * self.fps / fps), self.length - 1) for i in range(count)]


def tau(i, n):
    return 2 * math.pi * i / n


def hold(pose, n, **wobble):
    """A segment holding a pose, with optional sinusoidal wobble: each
    keyword is a field name mapped to (amplitude, period in frames, phase).
    The frames repeat with the wobble's period, so they render once."""

    def f(i):
        return add(
            pose, **{k: a * math.sin(tau(i, per) + ph) for k, (a, per, ph) in wobble.items()}
        )

    periods = [per for _, per, _ in wobble.values()]
    return Seg(n, f, cycle=math.lcm(*periods) if periods else 1)


def tween(frames, n):
    """A segment interpolating keyframes: list of (time 0..1, Pose)."""
    return Seg(n, lambda i: keyframes(frames, i / max(n - 1, 1)))


REST = Pose()


# ---- gaits


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
            l = -58 - 30 * lift
            setattr(p, name + "_u", u)
            setattr(p, name + "_l", l)
            setattr(p, name + "_f", lerp(32 - (u + l), 30, lift))
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
            l = -58 - 40 * lift
            setattr(p, name + "_u", u)
            setattr(p, name + "_l", l)
            setattr(p, name + "_f", lerp(32 - (u + l), 30, lift))
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


def walk(n, flip=False):
    return Seg(n, walk_pose, speed=-WALK_SPEED if flip else WALK_SPEED, cycle=24, flip=flip)


def run(n, flip=False, pose=run_pose):
    return Seg(n, pose, speed=-RUN_SPEED if flip else RUN_SPEED, cycle=16, flip=flip)


def walk_in(to_x):
    """From off the left edge to x."""
    return walk(int((to_x - OFF_LEFT) / WALK_SPEED))


def walk_out(from_x):
    return walk(int((OFF_RIGHT - from_x) / WALK_SPEED))


def run_in(to_x):
    return run(int((to_x - OFF_LEFT) / RUN_SPEED))


def run_out(from_x):
    return run(int((OFF_RIGHT - from_x) / RUN_SPEED))


def end_x(start_x, segs):
    """Where a show's body origin is after these segments."""
    return Anim("", start_x, segs + [Seg(1, lambda i: REST)]).path(sum(s.n for s in segs))[0]


# ---- named poses

# The classic sitting pose: rear on the ground, back sloping up to the head,
# front legs straight, hind legs folded under the haunch.
SIT = Pose(
    body_x=-6,
    body_y=20,
    body_angle=-32,
    head_angle=30,
    head_dx=-4,
    head_dy=-6,
    fn_u=-30,
    fn_l=0,
    ff_u=-35,
    ff_l=0,
    bn_u=16,
    bn_l=-104,
    bn_f=150,
    bf_u=12,
    bf_l=-104,
    bf_f=150,
    tail1=125,
    tail2=60,
    tail3=55,
    haunch=1,
)

# Sitting with the near front paw lifted to the mouth.
LICK = dataclasses.replace(
    SIT, fn_u=55, fn_l=-150, head_angle=58, head_dx=-6, head_dy=2, eye=0.3, tongue=1
)

# Sitting, swatting at something in front of the face.
SWAT = dataclasses.replace(SIT, fn_u=100, fn_l=-50, head_angle=10, mouth=0.3)

# The play bow: chest down, rump up, front legs stretched out ahead.
STRETCH = Pose(
    body_y=18,
    body_angle=22,
    head_angle=26,
    head_dy=8,
    fn_u=70,
    fn_l=-4,
    ff_u=64,
    ff_l=-4,
    bn_u=26,
    bn_l=-14,
    bn_f=22,
    bf_u=30,
    bf_l=-14,
    bf_f=18,
    tail1=8,
    tail2=10,
    tail3=10,
    eye=0,
)

# Standing, head thrown back in a yawn.
YAWN = Pose(head_angle=-28, head_dy=-4, mouth=1, eye=0, ear_near=-20, ear_far=-20)

# Lying sphinx-style, belly on the ground, front legs out ahead.
LOAF = Pose(
    body_y=46,
    head_angle=8,
    head_dy=6,
    fn_u=45,
    fn_l=45,
    ff_u=40,
    ff_l=50,
    bn_u=60,
    bn_l=-150,
    bn_f=160,
    bf_u=56,
    bf_l=-150,
    bf_f=160,
    tail1=150,
    tail2=40,
    tail3=50,
    haunch=1,
)
ASLEEP = dataclasses.replace(LOAF, eye=0, head_angle=18, head_dy=12, ear_near=-10, ear_far=-10)

# Up on the hind legs.
DANCE = Pose(
    body_y=-4,
    body_angle=-75,
    head_angle=66,
    head_dx=4,
    fn_u=75,
    fn_l=-20,
    ff_u=40,
    ff_l=-30,
    bn_u=-55,
    bn_l=-50,
    bn_f=62,
    bf_u=-50,
    bf_l=-50,
    bf_f=58,
    tail1=160,
    tail2=20,
    tail3=20,
    eye=0,
    happy=1,
    mouth=0.4,
)

# Startled: up on tiptoe, ears back, eyes wide, tail puffed straight up.
SCARED = Pose(
    body_y=-14,
    fn_u=2,
    fn_l=0,
    ff_u=-2,
    ff_l=0,
    bn_u=10,
    bn_l=-30,
    bn_f=52,
    bf_u=12,
    bf_l=-30,
    bf_f=50,
    tail1=0,
    tail2=0,
    tail3=0,
    puff=1.9,
    ear_near=-45,
    ear_far=-45,
    eye=1.3,
    pupil=2.6,
    mouth=0.6,
)

# Mid-leap: stretched out, nose up, front legs reaching, hind legs trailing.
LEAP = Pose(
    body_angle=-18,
    head_angle=10,
    fn_u=70,
    fn_l=-10,
    ff_u=60,
    ff_l=-10,
    bn_u=-30,
    bn_l=-20,
    bn_f=30,
    bf_u=-25,
    bf_l=-20,
    bf_f=30,
    tail1=95,
    tail2=5,
    tail3=5,
    ear_near=-10,
    ear_far=-10,
)
LAND = Pose(
    body_angle=14,
    head_angle=0,
    fn_u=30,
    fn_l=-10,
    ff_u=22,
    ff_l=-10,
    bn_u=50,
    bn_l=-95,
    bn_f=60,
    bf_u=46,
    bf_l=-95,
    bf_f=60,
    tail1=40,
    tail2=20,
    tail3=20,
)

# Crouched, ready to pounce.
CROUCH = Pose(
    body_y=22,
    fn_u=20,
    fn_l=-30,
    ff_u=16,
    ff_l=-30,
    bn_u=55,
    bn_l=-120,
    bn_f=120,
    bf_u=50,
    bf_l=-120,
    bf_f=120,
    tail1=120,
    tail2=10,
    tail3=10,
    head_dy=4,
    pupil=2,
    haunch=0.6,
)


# ---- the shows


def sit_groom_show():
    x = 300

    def lick_pose(i, n=12):
        lick = math.sin(tau(i, n))
        return add(
            LICK,
            head_angle=7 * lick,
            head_dy=2 * lick,
            fn_u=3 * lick,
            tail3=12 * math.sin(tau(i, n) - 1),
        )

    def look_pose(i, n=36):
        p = add(
            SIT,
            pupil_dx=-1.5,
            pupil_dy=1,
            tail1=6 * math.sin(tau(i, n)),
            tail3=12 * math.sin(tau(i, n) - 1),
        )
        blink = (i - 14) / 8
        if 0 <= blink <= 1:
            p.eye = 1 - math.sin(math.pi * blink)
        return p

    return Anim(
        "sit_groom",
        OFF_LEFT,
        [
            walk_in(x),
            tween([(0, walk_pose(0)), (1, SIT)], 12),
            hold(SIT, 12, tail3=(12, 12, 0)),
            tween([(0, SIT), (1, LICK)], 10),
            Seg(60, lick_pose, cycle=12),
            tween([(0, LICK), (1, SIT)], 10),
            Seg(36, look_pose),
            tween([(0, SIT), (1, walk_pose(0))], 12),
            walk_out(x),
        ],
    )


def chase_pose(i, n=16):
    p = run_pose(i, n)
    p.yarn = 1
    p.yarn_x = 160
    p.yarn_y = GROUND_Y - YARN_R - 6 * abs(math.sin(tau(i, n) * 1.5))
    p.yarn_angle = -360 * i / n * 1.5
    return p


def chase_show():
    n = int((OFF_RIGHT - OFF_LEFT) / RUN_SPEED)
    return Anim("chase", OFF_LEFT, [run(n, pose=chase_pose)])


def stretch_show():
    x = 300
    return Anim(
        "stretch",
        OFF_LEFT,
        [
            walk_in(x),
            tween([(0, walk_pose(0)), (0.5, REST), (1, STRETCH)], 20),
            hold(STRETCH, 30, tail3=(10, 30, 0)),
            tween([(0, STRETCH), (0.6, REST), (1, YAWN)], 24),
            hold(YAWN, 20),
            tween([(0, YAWN), (1, walk_pose(0))], 12),
            walk_out(x),
        ],
    )


def nap_show():
    x = 320

    def sleep_pose(i, n=48):
        breath = math.sin(tau(i, n))
        p = add(ASLEEP, body_y=1.2 * breath, head_dy=1.5 * breath)
        p.zzz = i / n
        twitch = (i - 30) / 6
        if 0 <= twitch < 1:
            p.ear_near = -10 + 25 * math.sin(math.pi * twitch)
        return p

    return Anim(
        "nap",
        OFF_LEFT,
        [
            walk_in(x),
            tween([(0, walk_pose(0)), (0.4, REST), (1, LOAF)], 24),
            hold(LOAF, 16),
            tween([(0, LOAF), (1, ASLEEP)], 24),
            Seg(144, sleep_pose, cycle=48),
            tween(
                [
                    (0, ASLEEP),
                    (0.5, LOAF),
                    (1, dataclasses.replace(LOAF, mouth=1, eye=0, head_angle=-20)),
                ],
                24,
            ),
            tween(
                [
                    (0, dataclasses.replace(LOAF, mouth=1, eye=0, head_angle=-20)),
                    (0.5, LOAF),
                    (1, STRETCH),
                ],
                24,
            ),
            hold(STRETCH, 16),
            tween([(0, STRETCH), (1, walk_pose(0))], 16),
            walk_out(x),
        ],
    )


def dance_show():
    x = 360

    def dance_pose(i, n=48):
        phi = tau(i, n)
        p = add(
            DANCE,
            body_angle=8 * math.sin(phi),
            body_x=10 * math.sin(phi),
            head_angle=-8 * math.sin(phi),
            head_dx=3 * math.sin(2 * phi),
            fn_u=25 * math.sin(phi),
            ff_u=-25 * math.sin(phi),
            tail1=25 * math.sin(phi - 0.5),
            tail3=20 * math.sin(phi - 1.5),
            body_y=-3 * abs(math.sin(2 * phi)),
        )
        return p

    return Anim(
        "dance",
        OFF_LEFT,
        [
            walk_in(x),
            tween([(0, walk_pose(0)), (0.3, REST), (1, DANCE)], 24),
            Seg(144, dance_pose, cycle=48),
            tween([(0, DANCE), (0.7, REST), (1, walk_pose(0))], 24),
            walk_out(x),
        ],
    )


def scared_show():
    x = 300
    jump_n = 10

    def hop(i):
        t = i / jump_n
        return 0, 40 * math.sin(math.pi * t)

    def bolt_pose(i):
        p = run_pose(i)
        p.puff = 1.5
        p.ear_near = p.ear_far = -35
        p.pupil = 2
        return p

    return Anim(
        "scared",
        OFF_LEFT,
        [
            walk_in(x),
            tween([(0, walk_pose(0)), (1, REST)], 8),
            hold(REST, 20, pupil_dx=(2, 40, 0)),
            Seg(jump_n, lambda i: SCARED, arc=hop, cycle=1),
            hold(SCARED, 22, body_x=(2, 4, 0)),
            tween([(0, SCARED), (1, bolt_pose(0))], 6),
            Seg(int((OFF_RIGHT - x) / 15), bolt_pose, speed=15, cycle=16),
        ],
    )


def zoomies_show():
    # a flat-out sprint: twice the run speed, strides twice as fast
    speed = 26
    n = int((OFF_RIGHT - OFF_LEFT) / speed)
    fast = lambda i: add(run_pose(2 * i), ear_near=-15, ear_far=-15, pupil=1.8, mouth=0.2)
    cross = Seg(n, fast, speed=speed, cycle=8)
    back = Seg(n, fast, speed=-speed, cycle=8, flip=True)
    return Anim("zoomies", OFF_LEFT, [cross, back, cross, back])


def peek_show():
    # only the head shows past the left edge; she looks around and ducks back
    x = -50
    peek_n = 24

    def look_pose(i, n=48):
        p = add(
            REST,
            pupil_dx=3 * math.sin(tau(i, n)),
            pupil_dy=1.5 * math.sin(tau(i, n // 2)),
            head_angle=6 * math.sin(tau(i, n)),
            ear_near=15 * max(0, math.sin(tau(i, n // 2))),
        )
        blink = (i - 20) / 8
        if 0 <= blink <= 1:
            p.eye = 1 - math.sin(math.pi * blink)
        return p

    slide = lambda i: ((x - OFF_LEFT) * smooth(i / peek_n) - (x - OFF_LEFT) * i / peek_n, 0)
    return Anim(
        "peek",
        OFF_LEFT,
        [
            Seg(peek_n, lambda i: REST, speed=(x - OFF_LEFT) / peek_n, arc=slide, cycle=1),
            Seg(96, look_pose, cycle=48),
            Seg(
                peek_n,
                lambda i: REST,
                speed=-(x - OFF_LEFT) / peek_n,
                arc=lambda i: slide(peek_n - i),
                cycle=1,
            ),
        ],
    )


def leap_show():
    x = 200
    air = 22
    span = 260

    def arc(i):
        t = i / air
        return span * t, 150 * math.sin(math.pi * t)

    return Anim(
        "leap",
        OFF_LEFT,
        [
            run_in(x),
            tween([(0, run_pose(0)), (1, CROUCH)], 6),
            Seg(
                air,
                lambda i: keyframes([(0, CROUCH), (0.25, LEAP), (0.8, LEAP), (1, LAND)], i / air),
                arc=arc,
            ),
            tween([(0, LAND), (1, run_pose(0))], 6),
            run_out(x + span),
        ],
    )


def pounce_show():
    x = 280
    wiggle_n = 48
    air = 18
    span = 170

    def wiggle_pose(i):
        p = add(
            CROUCH,
            body_angle=4 * math.sin(tau(i, 8)),
            body_x=3 * math.sin(tau(i, 8)),
            tail3=30 * math.sin(tau(i, 16)),
        )
        p.yarn = 1
        p.yarn_x = span + 10
        return p

    def with_yarn(pose, dx):
        q = dataclasses.replace(pose)
        q.yarn = 1
        q.yarn_x = span + 10 - dx
        return q

    def fly_pose(i):
        t = i / air
        p = keyframes([(0, CROUCH), (0.3, LEAP), (0.8, LEAP), (1, LAND)], t)
        return with_yarn(p, span * t)

    def arc(i):
        t = i / air
        return span * t, 90 * math.sin(math.pi * t)

    caught = with_yarn(dataclasses.replace(LAND, head_angle=35, head_dy=4, eye=0, happy=1), span)
    return Anim(
        "pounce",
        OFF_LEFT,
        [
            Seg(
                int((x - OFF_LEFT) / WALK_SPEED),
                lambda i: with_yarn(walk_pose(i), 0),
                speed=WALK_SPEED,
                cycle=24,
            ),
            tween([(0, with_yarn(walk_pose(0), 0)), (1, with_yarn(CROUCH, 0))], 10),
            Seg(wiggle_n, wiggle_pose),
            Seg(air, fly_pose, arc=arc),
            tween([(0, with_yarn(LAND, span)), (1, caught)], 10),
            hold(caught, 36, tail1=(15, 24, 0)),
        ],
    )


def butterfly_show():
    x = 300
    lap = 60  # one figure eight, with a swat
    leave = 40

    def loop_at(i):
        return 110 + 40 * math.sin(tau(i, lap)), -80 + 25 * math.sin(tau(i, lap // 2))

    def watching(p, bx, by, i):
        """Pose p with the head and eyes on the butterfly at (bx, by)."""
        p.head_angle += -0.25 * (by + 80)
        p.pupil_dx = max(-3, min(3, (bx - 120) / 12))
        p.pupil_dy = max(-3, min(3, (by + 60) / 20))
        p.bfly = 1
        p.bfly_x, p.bfly_y = bx, by
        p.bfly_wing = i / 6
        p.tail3 += 15 * math.sin(tau(i, lap // 2))
        return p

    def lap_pose(i):
        u = (i - 36) / 14
        p = blend(SIT, SWAT, math.sin(math.pi * u)) if 0 <= u <= 1 else dataclasses.replace(SIT)
        return watching(p, *loop_at(i), i)

    def leave_pose(i):
        u = i / leave
        bx, by = loop_at(0)
        p = watching(dataclasses.replace(SIT), bx + 260 * u, by - 160 * u, i)
        if u > 0.8:
            p.pupil_dx, p.pupil_dy = -1.5, 1
            p.bfly = 0
        return p

    return Anim(
        "butterfly",
        OFF_LEFT,
        [
            walk_in(x),
            tween([(0, walk_pose(0)), (1, SIT)], 12),
            Seg(2 * lap, lap_pose, cycle=lap),
            Seg(leave, leave_pose),
            hold(dataclasses.replace(SIT, pupil_dx=-1.5, pupil_dy=1), 20),
            tween([(0, SIT), (1, walk_pose(0))], 12),
            walk_out(x),
        ],
    )


def walk_show():
    return Anim("walk", OFF_LEFT, [walk(int((OFF_RIGHT - OFF_LEFT) / WALK_SPEED))])


def run_show():
    return Anim("run", OFF_LEFT, [run(int((OFF_RIGHT - OFF_LEFT) / RUN_SPEED))])


SHOWS = [
    walk_show,
    run_show,
    chase_show,
    sit_groom_show,
    stretch_show,
    nap_show,
    dance_show,
    scared_show,
    zoomies_show,
    peek_show,
    leap_show,
    pounce_show,
    butterfly_show,
]

# Frames are rendered on a canvas big enough for any pose, then cropped to
# the union of the animation's frames. In output pixels at cat_scale 1.
CANVAS = (440, 340)
CANVAS_ORIGIN = (220, 200)


# ---------------------------------------------------------------- outputs


@dataclasses.dataclass
class Frames:
    """A show rendered: the unique images (mirrored ones included), and for
    each played frame which image to show and where its top-left corner
    goes on the screen, in pixels."""

    images: list
    placement: list  # (image number, left, top) per frame
    fps: int


def frames_of(anim, fps=FPS, cat_scale=1.0, pad=2):
    w, h = CANVAS
    size = (int(w * cat_scale), int(h * cat_scale))
    origin = (CANVAS_ORIGIN[0] * cat_scale, CANVAS_ORIGIN[1] * cat_scale)
    timeline = anim.sample(fps)

    # render each distinct pose once
    rendered, by_key = [], {}
    for t in timeline:
        key = anim.key(t)
        if key not in by_key:
            by_key[key] = len(rendered)
            rendered.append(render(anim.pose(t), size, origin, cat_scale))
    boxes = [img.getbbox() for img in rendered]
    x0 = max(0, min(b[0] for b in boxes) - pad)
    y0 = max(0, min(b[1] for b in boxes) - pad)
    x1 = min(size[0], max(b[2] for b in boxes) + pad)
    y1 = min(size[1], max(b[3] for b in boxes) + pad)
    rendered = [img.crop((x0, y0, x1, y1)) for img in rendered]
    origin_x, origin_y = origin[0] - x0, origin[1] - y0
    width = x1 - x0

    # mirrored frames get their own images, so the player never transforms
    images, by_image, placement = [], {}, []
    for t in timeline:
        x, y, flip = anim.path(t)
        k = (by_key[anim.key(t)], flip)
        if k not in by_image:
            by_image[k] = len(images)
            img = rendered[k[0]]
            images.append(img.transpose(Image.FLIP_LEFT_RIGHT) if flip else img)
        ox = width - origin_x if flip else origin_x
        left = round(x * cat_scale - ox)
        top = round(GROUND_LINE - y * cat_scale - origin_y - GROUND_Y * cat_scale)
        placement.append((by_image[k], left, top))
    return Frames(images, placement, fps)


def on_background(img, color=(120, 170, 220, 255)):
    bg = Image.new("RGBA", img.size, color)
    bg.alpha_composite(img)
    return bg


def write_gif(path, frames, fps):
    frames = [on_background(f).convert("RGB") for f in frames]
    frames[0].save(
        path,
        save_all=True,
        append_images=frames[1:],
        duration=int(1000 / fps),
        loop=0,
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


def write_scene(path, frames, shrink=2):
    """The show on a screen-sized canvas. The GIF is written at 1/shrink of
    the screen size."""
    out = []
    for image, left, top in frames.placement:
        img = frames.images[image]
        bg = Image.new("RGBA", (SCREEN_W, SCREEN_H), (30, 34, 60, 255))
        if left < SCREEN_W and top < SCREEN_H and left + img.width > 0 and top + img.height > 0:
            bg.alpha_composite(
                img, dest=(max(left, 0), max(top, 0)), source=(max(-left, 0), max(-top, 0))
            )
        out.append(
            bg.convert("RGB").resize((SCREEN_W // shrink, SCREEN_H // shrink), Image.LANCZOS)
        )
    out[0].save(path, save_all=True, append_images=out[1:], duration=int(1000 / frames.fps), loop=0)


def export(directory, shows, fps, cat_scale):
    """Writes every show's images as PNGs under directory and a manifest.json
    the clock build turns into frame tables."""
    manifest = {"fps": fps, "screen": [SCREEN_W, SCREEN_H], "shows": []}
    for anim, frames in shows:
        os.makedirs(os.path.join(directory, anim.name), exist_ok=True)
        paths = []
        for i, img in enumerate(frames.images):
            rel = os.path.join(anim.name, f"{i:03d}.png")
            img.save(os.path.join(directory, rel))
            paths.append(rel)
        manifest["shows"].append({
            "name": anim.name,
            "images": paths,
            "frames": [list(p) for p in frames.placement],
        })
    with open(os.path.join(directory, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--preview", metavar="DIR", help="write GIFs and contact strips")
    ap.add_argument("--export", metavar="DIR", help="write PNG frames and a manifest")
    ap.add_argument("--fps", type=int, default=12, help="playback frame rate")
    ap.add_argument("--scale", type=float, default=1.0, help="cat scale factor")
    ap.add_argument("--only", action="append", help="limit to these shows")
    args = ap.parse_args()

    shows = []
    for show in SHOWS:
        anim = show()
        if args.only and anim.name not in args.only:
            continue
        frames = frames_of(anim, args.fps, args.scale)
        shows.append((anim, frames))
        if args.preview:
            os.makedirs(args.preview, exist_ok=True)
            write_gif(os.path.join(args.preview, anim.name + ".gif"), frames.images, frames.fps)
            write_strip(os.path.join(args.preview, anim.name + "_strip.png"), frames.images)
            write_scene(os.path.join(args.preview, anim.name + "_scene.gif"), frames)
        w, h = frames.images[0].size
        print(
            f"{anim.name}: {len(frames.placement)} frames ({len(frames.images)} images) "
            f"@ {frames.fps} fps, {w}x{h}, {len(frames.placement) / frames.fps:.1f} s"
        )
    if args.export:
        export(args.export, shows, args.fps, args.scale)


if __name__ == "__main__":
    main()
