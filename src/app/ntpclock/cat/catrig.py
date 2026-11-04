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

    catrig.py --preview DIR      GIF and contact strip per animation
    catrig.py --frames DIR       PNG frame sequences per animation
"""

import argparse
import dataclasses
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
        g.stroke(apply(N, [(cx - r, cy), (cx, cy + 3), (cx + r, cy)]), width=2.0)
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
    widths = [TAIL_SEGS[0][1] + 2]
    for (length, width), angle in zip(TAIL_SEGS, (pose.tail1, pose.tail2, pose.tail3)):
        T = chain(T, rot(-angle), trans(0, -length))
        joints.append(apply(T, [(0, 0)])[0])
        widths.append(width)
    s_tail = tail.shape(tube(joints, widths, tuft=3), BLACK)
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

# Frames are rendered on a canvas big enough for any pose, then cropped to
# the union of the animation's frames. In output pixels at cat_scale 1.
CANVAS = (420, 280)
CANVAS_ORIGIN = (210, 140)


# ---------------------------------------------------------------- outputs


@dataclasses.dataclass
class Frames:
    images: list
    # where the body center at rest sits in each image, in pixels
    origin: tuple
    # the ground line, in pixels below the top of each image
    ground: float


def frames_of(anim, cat_scale=1.0, pad=2):
    w, h = CANVAS
    size = (int(w * cat_scale), int(h * cat_scale))
    origin = (CANVAS_ORIGIN[0] * cat_scale, CANVAS_ORIGIN[1] * cat_scale)
    images = [render(anim.pose(i), size, origin, cat_scale) for i in range(anim.frames)]
    boxes = [img.getbbox() for img in images]
    x0 = max(0, min(b[0] for b in boxes) - pad)
    y0 = max(0, min(b[1] for b in boxes) - pad)
    x1 = min(size[0], max(b[2] for b in boxes) + pad)
    y1 = min(size[1], max(b[3] for b in boxes) + pad)
    return Frames(
        [img.crop((x0, y0, x1, y1)) for img in images],
        (origin[0] - x0, origin[1] - y0),
        origin[1] - y0 + GROUND_Y * cat_scale,
    )


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
    """The animation translating across a screen-sized canvas, feet on a
    ground line near the bottom."""
    out = []
    n = len(frames.images)
    fw, fh = frames.images[0].size
    total = int((width + fw) / anim.speed) if anim.speed else n * 2
    top = int(height - 24 - frames.ground)
    for k in range(total):
        bg = Image.new("RGBA", (width, height), (30, 34, 60, 255))
        x = int(-fw + k * anim.speed) if anim.speed else (width - fw) // 2
        bg.alpha_composite(frames.images[k % n], dest=(x, top))
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
            write_gif(
                os.path.join(args.preview, anim.name + ".gif"), frames.images, anim.fps, anim.loop
            )
            write_strip(os.path.join(args.preview, anim.name + "_strip.png"), frames.images)
            write_scene(os.path.join(args.preview, anim.name + "_scene.gif"), anim, frames)
        if args.frames:
            d = os.path.join(args.frames, anim.name)
            os.makedirs(d, exist_ok=True)
            for i, f in enumerate(frames.images):
                f.save(os.path.join(d, f"{i:03d}.png"))
        w, h = frames.images[0].size
        print(
            f"{anim.name}: {anim.frames} frames @ {anim.fps} fps, {w}x{h}, "
            f"origin {frames.origin[0]:.0f},{frames.origin[1]:.0f}"
        )


if __name__ == "__main__":
    main()
