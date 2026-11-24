#!/usr/bin/env python3
"""
BMO PLAYER - parametric case generator (CAD as code).

Single source of truth for every STL in ../STL. Geometry is modelled as signed
distance fields (SDF) on a voxel grid and meshed with marching cubes
(scikit-image).  Only numpy / scipy / scikit-image / Pillow are required.

Run:   python3 bmo_case.py            -> writes ../STL/*.stl, ../renders/*.png,
                                         ../../tests/fit_report.json
       python3 bmo_case.py --quick    -> coarse grid (fast preview)

GLOBAL COORDINATES (mm)
  X -> right when looking at the FRONT of the device, Y -> up, Z -> from the
  front face (z=0) towards the back face (z=T).
  Front shell prints face-down (as modelled).  Back shell is rotated 180 deg
  about Y for printing (back face on the bed).  Tray is rotated likewise.

!! Every number in the "MODULE DIMENSIONS" block is a NOMINAL value taken from
!! typical datasheets / listings.  Modules from different sellers differ by
!! +-1 mm.  MEASURE YOUR PARTS WITH CALIPERS and edit the block, then re-run.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from skimage import measure

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_STL = os.path.join(HERE, "..", "STL")
OUT_REN = os.path.join(HERE, "..", "renders")
OUT_TEST = os.path.join(HERE, "..", "..", "tests")
QUICK = "--quick" in sys.argv
H_SHELL = 0.55 if QUICK else 0.40   # voxel size, big parts (mm)
H_SMALL = 0.40 if QUICK else 0.25   # voxel size, small parts (mm)
BIG = 1e3
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

# ----------------------------------------------------------------------------
# CASE PARAMETERS
# ----------------------------------------------------------------------------
W, H, T = 76.0, 110.0, 26.0          # body: 76 x 110 x 26 (ratio 1:1.447, ref photo 5.5:8 = 1:1.455)
R = 8.0                              # plan-view corner radius
WALL = 2.0                           # wall thickness
RE = 1.0                             # edge rounding radius (outer)
SPLIT = 13.0                         # z of the parting plane
TONGUE_T, TONGUE_H, TONGUE_CLR = 1.0, 1.8, 0.2   # back-shell lip that enters the front shell
CX = W / 2

# front-face layout ---------------------------------------------------------
DISP_C = (38.0, 76.5)                # display pocket centre
WIN = (44.0, 33.0, 2.0)              # window w,h,corner r (active area 40.8x30.6 + margin)
BEZEL = (58.0, 41.0, 5.0)            # decorative recessed bezel
BEZEL_DEPTH = 0.6
DPAD_C, DPAD_PITCH = (22.0, 34.0), 11.0
KEY_UP = (DPAD_C[0], DPAD_C[1] + DPAD_PITCH)
KEY_DOWN = (DPAD_C[0], DPAD_C[1] - DPAD_PITCH)
KEY_LEFT = (DPAD_C[0] - DPAD_PITCH, DPAD_C[1])
KEY_RIGHT = (DPAD_C[0] + DPAD_PITCH, DPAD_C[1])
KEY_TRI = (52.0, 44.0)               # blue triangle  -> MODE
KEY_GREEN = (64.0, 33.0)             # green circle   -> OK / play-pause
KEY_RED = (54.0, 20.0)               # red circle     -> BACK
SQ_SHAFT, SQ_HOLE, SQ_FLANGE = 8.0, 8.5, 10.4
GR_SHAFT, GR_HOLE, GR_FLANGE = 8.0, 8.5, 10.4      # diameters
RD_SHAFT, RD_HOLE, RD_FLANGE = 10.0, 10.5, 12.5
TRI_RIN_SHAFT, TRI_RIN_HOLE, TRI_R_FLANGE = 3.4, 3.65, 8.0
CAP_PROTRUDE, FLANGE_T = 0.8, 1.2
KEYS = {  # name: (center, kind)
    "up": (KEY_UP, "sq"), "down": (KEY_DOWN, "sq"), "left": (KEY_LEFT, "sq"),
    "right": (KEY_RIGHT, "sq"), "triangle": (KEY_TRI, "tri"),
    "green": (KEY_GREEN, "gr"), "red": (KEY_RED, "rd"),
}

# fastening ------------------------------------------------------------------
CLOSE_BOSS = [(8.0, 8.0), (68.0, 8.0), (8.0, 102.0), (68.0, 102.0)]   # M2 x 18 from the back
TRAY_BOSS = [(22.0, 7.0), (54.0, 7.0), (12.0, 54.0), (64.0, 54.0)]    # M2 x 6 (tray)
BOSS_R, BOSS_PILOT, SCREW_CLR = 3.6, 1.8, 2.4
TRAY_Z0, TRAY_Z1 = 7.6, 9.2          # tray plate (front face of plate at 7.6)

# ----------------------------------------------------------------------------
# MODULE DIMENSIONS (NOMINAL - VERIFY WITH CALIPERS)  x0,x1,y0,y1,z0,z1 global
# ----------------------------------------------------------------------------
SW_BODY = 6.0           # tact switch 6x6, total height 4.3 incl. plunger (THT, pins 4.5 x 6.5)
SW_H = 4.3
SW_PIN = (4.5, 6.5)
MOD = {
    "display_2.0_ST7789":   (12.0, 64.0, 58.5, 94.5, 2.0, 6.0),     # 52 x 36 x 4 (PCB+glass)
    "esp32_devkit":         (4.5, 56.0, 62.35, 90.65, 8.4, 13.2),   # 51.5 x 28.3 x 4.8, no header pins
    "tp4056_usbc":          (13.1, 30.1, 2.2, 28.2, 9.4, 14.0),     # 17 x 26 x 4.6, USB-C towards -Y
    "boost_5v":             (12.5, 25.2, 32.0, 49.8, 9.4, 13.0),    # Pololu U1V10F5 12.7 x 17.8
    "dac_pcm5102_jack":     (45.3, 62.8, 2.2, 33.7, 9.4, 16.0),     # 17.5 x 31.5, 3.5 mm jack towards -Y
    "microsd_module":       (47.0, 72.0, 36.0, 55.0, 9.4, 13.4),    # 25 x 19 x 4, slot towards +X
    "slide_switch":         (2.2, 5.8, 31.75, 40.25, 9.8, 13.3),    # SS12D00 class 8.5 x 3.6 x 3.5
    "lipo_603450":          (21.0, 55.0, 12.5, 62.5, 17.5, 23.5),   # 34 x 50 x 6
}
FRAME_MODS = {  # modules that get a printed cradle on the back of the tray: (name, open side)
    "tp4056_usbc": "-y", "boost_5v": None, "dac_pcm5102_jack": "-y", "microsd_module": "+x",
}
FRAME_WALL, FRAME_CLR, FRAME_H = 1.0, 0.3, 3.0

# connector openings (global) ----------------------------------------------------
USBC = dict(x=21.6, z=12.2, w=9.6, h=4.0)            # charge port, bottom wall
JACK = dict(x=54.05, z=14.0, d=8.2)                  # 3.5 mm jack, bottom wall (straddles the parting plane)
SDSLOT = dict(y=45.5, z=12.0, w=12.8, h=2.8)         # microSD slot, right wall
MICROUSB = dict(y=76.5, z=10.8, w=10.4, h=5.2)       # ESP32 programming port, left wall
SLIDE = dict(y=36.0, z=11.0, w=7.2, h=2.6)           # power slide switch, left wall

# ----------------------------------------------------------------------------
# SDF primitives
# ----------------------------------------------------------------------------
def box(X, Y, Z, x0, x1, y0, y1, z0, z1):
    qx = np.abs(X - (x0 + x1) / 2) - (x1 - x0) / 2
    qy = np.abs(Y - (y0 + y1) / 2) - (y1 - y0) / 2
    qz = np.abs(Z - (z0 + z1) / 2) - (z1 - z0) / 2
    out = np.sqrt(np.maximum(qx, 0) ** 2 + np.maximum(qy, 0) ** 2 + np.maximum(qz, 0) ** 2)
    return out + np.minimum(np.maximum(np.maximum(qx, qy), qz), 0)


def rr2(A, B, ca, cb, w, h, r):
    qa = np.abs(A - ca) - (w / 2 - r)
    qb = np.abs(B - cb) - (h / 2 - r)
    return np.sqrt(np.maximum(qa, 0) ** 2 + np.maximum(qb, 0) ** 2) + np.minimum(np.maximum(qa, qb), 0) - r


def circ2(A, B, ca, cb, r):
    return np.sqrt((A - ca) ** 2 + (B - cb) ** 2) - r


def tri2(A, B, ca, cb, rin):
    """equilateral triangle pointing +B (up), inradius rin"""
    s3 = math.sqrt(3) / 2
    p, q = A - ca, B - cb
    return np.maximum(np.maximum(-q, s3 * p + 0.5 * q), -s3 * p + 0.5 * q) - rin


def ext(d2, dn):
    return np.sqrt(np.maximum(d2, 0) ** 2 + np.maximum(dn, 0) ** 2) + np.minimum(np.maximum(d2, dn), 0)


def dz_(Z, z0, z1):
    return np.maximum(z0 - Z, Z - z1)


# ----------------------------------------------------------------------------
# voxel grid with local CSG
# ----------------------------------------------------------------------------
class G:
    def __init__(self, bb, h):
        x0, x1, y0, y1, z0, z1 = bb
        self.h, self.x0, self.y0, self.z0 = h, x0, y0, z0
        self.nx = int(round((x1 - x0) / h)) + 1
        self.ny = int(round((y1 - y0) / h)) + 1
        self.nz = int(round((z1 - z0) / h)) + 1
        self.xs = x0 + h * np.arange(self.nx)
        self.ys = y0 + h * np.arange(self.ny)
        self.zs = z0 + h * np.arange(self.nz)
        self.d = np.full((self.nx, self.ny, self.nz), BIG, np.float32)

    def reg(self, b, pad=None):
        h = self.h
        pad = 3 * h if pad is None else pad
        x0, x1, y0, y1, z0, z1 = b
        i0 = max(0, int(math.floor((x0 - pad - self.x0) / h)))
        i1 = min(self.nx, int(math.ceil((x1 + pad - self.x0) / h)) + 1)
        j0 = max(0, int(math.floor((y0 - pad - self.y0) / h)))
        j1 = min(self.ny, int(math.ceil((y1 + pad - self.y0) / h)) + 1)
        k0 = max(0, int(math.floor((z0 - pad - self.z0) / h)))
        k1 = min(self.nz, int(math.ceil((z1 + pad - self.z0) / h)) + 1)
        if i1 <= i0 or j1 <= j0 or k1 <= k0:
            return None
        return ((slice(i0, i1), slice(j0, j1), slice(k0, k1)),
                self.xs[i0:i1][:, None, None], self.ys[j0:j1][None, :, None], self.zs[k0:k1][None, None, :])

    def add(self, b, fn):
        r = self.reg(b)
        if r is None:
            return
        sl, X, Y, Z = r
        self.d[sl] = np.minimum(self.d[sl], fn(X, Y, Z).astype(np.float32))

    def cut(self, b, fn):
        r = self.reg(b)
        if r is None:
            return
        sl, X, Y, Z = r
        self.d[sl] = np.maximum(self.d[sl], -fn(X, Y, Z).astype(np.float32))

    # conveniences ------------------------------------------------------------
    def addbox(self, x0, x1, y0, y1, z0, z1):
        self.add((x0, x1, y0, y1, z0, z1), lambda X, Y, Z: box(X, Y, Z, x0, x1, y0, y1, z0, z1))

    def cutbox(self, x0, x1, y0, y1, z0, z1):
        self.cut((x0, x1, y0, y1, z0, z1), lambda X, Y, Z: box(X, Y, Z, x0, x1, y0, y1, z0, z1))

    def addcylz(self, cx, cy, r, z0, z1):
        self.add((cx - r, cx + r, cy - r, cy + r, z0, z1),
                 lambda X, Y, Z: ext(circ2(X, Y, cx, cy, r), dz_(Z, z0, z1)))

    def cutcylz(self, cx, cy, r, z0, z1):
        self.cut((cx - r, cx + r, cy - r, cy + r, z0, z1),
                 lambda X, Y, Z: ext(circ2(X, Y, cx, cy, r), dz_(Z, z0, z1)))

    def cutcyly(self, cx, cz, r, y0, y1):
        self.cut((cx - r, cx + r, y0, y1, cz - r, cz + r),
                 lambda X, Y, Z: ext(circ2(X, Z, cx, cz, r), dz_(Y, y0, y1)))

    def cutrrz(self, cx, cy, w, h, r, z0, z1):
        self.cut((cx - w / 2, cx + w / 2, cy - h / 2, cy + h / 2, z0, z1),
                 lambda X, Y, Z: ext(rr2(X, Y, cx, cy, w, h, r), dz_(Z, z0, z1)))

    def addrrz(self, cx, cy, w, h, r, z0, z1):
        self.add((cx - w / 2, cx + w / 2, cy - h / 2, cy + h / 2, z0, z1),
                 lambda X, Y, Z: ext(rr2(X, Y, cx, cy, w, h, r), dz_(Z, z0, z1)))

    def clip_z(self, zmin=None, zmax=None):
        Z = self.zs[None, None, :]
        if zmax is not None:
            self.d = np.maximum(self.d, (Z - zmax).astype(np.float32))
        if zmin is not None:
            self.d = np.maximum(self.d, (zmin - Z).astype(np.float32))

    def cut_text(self, text, cx, cy, height, max_w, z0, z1, flip_x=False):
        """engrave text (pseudo-SDF from anti-aliased coverage)"""
        h = self.h
        font = ImageFont.truetype(FONT, 120)
        l, t, r_, b = font.getbbox(text)
        img = Image.new("L", (r_ - l + 8, b - t + 8), 0)
        ImageDraw.Draw(img).text((4 - l, 4 - t), text, font=font, fill=255)
        bb = img.getbbox()
        img = img.crop(bb)
        aspect = img.width / img.height
        hh = height
        ww = hh * aspect
        if ww > max_w:
            ww, hh = max_w, max_w / aspect
        nxp, nyp = max(2, int(round(ww / h))), max(2, int(round(hh / h)))
        cov = np.asarray(img.resize((nxp, nyp), Image.LANCZOS), np.float32).T / 255.0   # [x, y_img]
        cov = cov[:, ::-1]                                                              # y up
        if flip_x:
            cov = cov[::-1, :]
        i0 = int(round((cx - nxp * h / 2 - self.x0) / h))
        j0 = int(round((cy - nyp * h / 2 - self.y0) / h))
        T2 = np.full((self.nx, self.ny), BIG, np.float32)
        ia, ib = max(i0, 0), min(i0 + nxp, self.nx)
        ja, jb = max(j0, 0), min(j0 + nyp, self.ny)
        T2[ia:ib, ja:jb] = ((0.5 - cov[ia - i0:ib - i0, ja - j0:jb - j0]) * h * 1.5).astype(np.float32)
        r = self.reg((self.xs[ia], self.xs[ib - 1], self.ys[ja], self.ys[jb - 1], z0, z1))
        if r is None:
            return
        sl, X, Y, Z = r
        v = ext(T2[sl[0], sl[1]][:, :, None], dz_(Z, z0, z1))
        self.d[sl] = np.maximum(self.d[sl], -v.astype(np.float32))

    # output ----------------------------------------------------------------------
    def mesh(self):
        d = np.pad(self.d, 1, constant_values=BIG)
        v, f, _, _ = measure.marching_cubes(d, 0.0, spacing=(self.h,) * 3)
        v = v + np.array([self.x0, self.y0, self.z0]) - self.h
        # make sure triangles are CCW seen from outside (positive volume)
        a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
        if np.sum(np.einsum("ij,ij->i", a, np.cross(b, c))) < 0:
            f = f[:, ::-1]
        return v.astype(np.float32), f

    def volume_mm3(self):
        return float(np.count_nonzero(self.d < 0)) * self.h ** 3


def write_stl(path, v, f, xf=None):
    if xf is not None:
        v = xf(v.astype(np.float64)).astype(np.float32)
        a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
        if np.sum(np.einsum("ij,ij->i", a, np.cross(b, c))) < 0:
            f = f[:, ::-1]
    tri = v[f]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    ln = np.linalg.norm(n, axis=1, keepdims=True)
    n = np.where(ln > 0, n / np.maximum(ln, 1e-12), 0).astype(np.float32)
    rec = np.zeros(len(f), dtype=[("n", "<f4", 3), ("v", "<f4", (3, 3)), ("a", "<u2")])
    rec["n"], rec["v"] = n, tri
    with open(path, "wb") as fh:
        fh.write(b"BMO PLAYER @frandorturtles".ljust(80, b" "))
        fh.write(np.uint32(len(f)).tobytes())
        fh.write(rec.tobytes())
    mn, mx = v.min(0), v.max(0)
    vol = float(np.sum(np.einsum("ij,ij->i", tri[:, 0].astype(np.float64),
                                 np.cross(tri[:, 1].astype(np.float64), tri[:, 2].astype(np.float64)))) / 6.0)
    return dict(file=os.path.basename(path), triangles=int(len(f)), size_mb=round(os.path.getsize(path) / 1e6, 1),
                bbox_mm=[round(float(x), 2) for x in (mx - mn)], volume_mm3=round(vol, 1))


# ----------------------------------------------------------------------------
# shared shapes
# ----------------------------------------------------------------------------
def outer_d(X, Y, Z):
    """rounded-edge body, global frame"""
    w, h_, r = W - 2 * RE, H - 2 * RE, R - RE
    d2 = rr2(X, Y, CX, H / 2, w, h_, r)
    return ext(d2, dz_(Z, RE, T - RE)) - RE


def cavity_d(X, Y, Z, shrink=0.0):
    d2 = rr2(X, Y, CX, H / 2, W - 2 * WALL - 2 * shrink, H - 2 * WALL - 2 * shrink, R - WALL - shrink)
    return ext(d2, dz_(Z, WALL, T - WALL))


def connector_cuts(g):
    """all side-wall openings, extended inward so they also open the lip"""
    # USB-C charge port, bottom wall (y=0)
    g.cutbox(USBC["x"] - USBC["w"] / 2, USBC["x"] + USBC["w"] / 2, -1.5, 5.5,
             USBC["z"] - USBC["h"] / 2, USBC["z"] + USBC["h"] / 2)
    # 3.5 mm headphone jack, bottom wall
    g.cutcyly(JACK["x"], JACK["z"], JACK["d"] / 2, -1.5, 5.5)
    # microSD slot, right wall (x=W)
    g.cutbox(W - 5.0, W + 1.5, SDSLOT["y"] - SDSLOT["w"] / 2, SDSLOT["y"] + SDSLOT["w"] / 2,
             SDSLOT["z"] - SDSLOT["h"] / 2, SDSLOT["z"] + SDSLOT["h"] / 2)
    # ESP32 micro-USB (programming), left wall (x=0)
    g.cutbox(-1.5, 5.0, MICROUSB["y"] - MICROUSB["w"] / 2, MICROUSB["y"] + MICROUSB["w"] / 2,
             MICROUSB["z"] - MICROUSB["h"] / 2, MICROUSB["z"] + MICROUSB["h"] / 2)
    # power slide switch, left wall
    g.cutbox(-1.5, 4.0, SLIDE["y"] - SLIDE["w"] / 2, SLIDE["y"] + SLIDE["w"] / 2,
             SLIDE["z"] - SLIDE["h"] / 2, SLIDE["z"] + SLIDE["h"] / 2)


# ----------------------------------------------------------------------------
# parts
# ----------------------------------------------------------------------------
def lip_notches(g):
    """remove the thin lip where modules sit right against a wall"""
    z0, z1 = SPLIT - TONGUE_H - 0.5, SPLIT + 1.0
    for name in ("tp4056_usbc", "dac_pcm5102_jack"):
        x0, x1 = MOD[name][0] - 0.3, MOD[name][1] + 0.3
        g.cutbox(x0, x1, WALL, 4.2, z0, z1)
    y0, y1 = MOD["slide_switch"][2] - 0.3, MOD["slide_switch"][3] + 0.3
    g.cutbox(WALL, 4.4, y0, y1, z0, z1)


def build_front(h=H_SHELL):
    g = G((-1, W + 1, -1, 119, -1, SPLIT + 1.0), h)
    g.add((-1, W + 1, -1, H + 1, -1, SPLIT + 1), lambda X, Y, Z: np.maximum(outer_d(X, Y, Z), Z - SPLIT))
    g.cut((0, W, 0, H, WALL, SPLIT + 1), lambda X, Y, Z: cavity_d(X, Y, Z))
    # key-ring eyelet (like the keychain in the reference)
    g.addcylz(63.0, 112.5, 5.0, 0.0, 3.2)
    g.cutcylz(63.0, 112.5, 2.3, -1, 4)
    # display pocket (walls)
    px, py = DISP_C
    pw, ph = 52.0 + 0.8, 36.0 + 0.8
    g.add((px - pw / 2 - 1.2, px + pw / 2 + 1.2, py - ph / 2 - 1.2, py + ph / 2 + 1.2, 1.5, 7.2),
          lambda X, Y, Z: box(X, Y, Z, px - pw / 2 - 1.2, px + pw / 2 + 1.2, py - ph / 2 - 1.2, py + ph / 2 + 1.2, 1.5, 7.2))
    g.cutbox(px - pw / 2, px + pw / 2, py - ph / 2, py + ph / 2, WALL, 9.0)
    # retaining tabs over the display (module thickness 4.0 -> tabs 0.2 above), 4 tabs
    for sx in (-16, 16):
        g.addbox(px + sx - 3.5, px + sx + 3.5, py + ph / 2 - 1.2, py + ph / 2 + 0.2, 6.2, 7.2)
        g.addbox(px + sx - 3.5, px + sx + 3.5, py - ph / 2 - 0.2, py - ph / 2 + 1.2, 6.2, 7.2)
    # ESP32 ledges (rest on the pocket walls) + left posts, ESP32 sits at z=8.4
    g.addbox(18.0, 56.0, py - ph / 2 - 1.2, py - ph / 2 + 5.1, 7.2, 8.4)
    g.addbox(18.0, 56.0, py + ph / 2 - 5.1, py + ph / 2 + 1.2, 7.2, 8.4)
    g.addbox(3.0, 9.5, 62.0, 66.5, WALL - 0.5, 8.4)
    g.addbox(3.0, 9.5, 86.5, 91.0, WALL - 0.5, 8.4)
    # wire notch for the display header (left side)
    g.cutbox(px - pw / 2 - 1.5, px - pw / 2 + 0.2, py - 6.5, py + 6.5, WALL, 7.5)
    # closing bosses + pilot holes (M2 self-tapping)
    for (bx, by) in CLOSE_BOSS:
        g.addcylz(bx, by, BOSS_R, WALL - 0.5, SPLIT)
        g.cutcylz(bx, by, BOSS_PILOT / 2, 3.5, SPLIT + 0.5)
    # tray bosses
    for (bx, by) in TRAY_BOSS:
        g.addcylz(bx, by, 3.2, WALL - 0.5, TRAY_Z0)
        g.cutcylz(bx, by, BOSS_PILOT / 2, 3.0, TRAY_Z0 + 0.5)
    # window + bezel
    g.cutrrz(px, py, WIN[0], WIN[1], WIN[2], -1.0, WALL + 1.0)
    g.cutrrz(px, py, BEZEL[0], BEZEL[1], BEZEL[2], -1.0, BEZEL_DEPTH)
    # key holes
    for name, ((kx, ky), kind) in KEYS.items():
        if kind == "sq":
            g.cutrrz(kx, ky, SQ_HOLE, SQ_HOLE, 0.6, -1.0, WALL + 1.0)
        elif kind == "gr":
            g.cutcylz(kx, ky, GR_HOLE / 2, -1.0, WALL + 1.0)
        elif kind == "rd":
            g.cutcylz(kx, ky, RD_HOLE / 2, -1.0, WALL + 1.0)
        else:
            g.cut((kx - 8, kx + 8, ky - 8, ky + 8, -1.0, WALL + 1.0),
                  lambda X, Y, Z, kx=kx, ky=ky: ext(tri2(X, Y, kx, ky, TRI_RIN_HOLE), dz_(Z, -1.0, WALL + 1.0)))
    # speaker dimples (decorative, like the reference) and label
    for i in range(5):
        g.cutcylz(70.6, 80.0 + 3.6 * i, 0.8, -1.0, 0.6)
    g.cut_text("BMO", 38.0, 9.0, 6.0, 26.0, -1.0, 0.4)
    connector_cuts(g)
    return g


def build_back(h=H_SHELL):
    g = G((-1, W + 1, -1, H + 1, 10.4, T + 1), h)
    g.add((-1, W + 1, -1, H + 1, SPLIT, T + 1), lambda X, Y, Z: np.maximum(outer_d(X, Y, Z), SPLIT - Z))
    g.cut((0, W, 0, H, SPLIT - 1, T - WALL), lambda X, Y, Z: cavity_d(X, Y, Z))
    # lip (tongue) that enters the front shell
    z0, z1 = SPLIT - TONGUE_H, SPLIT + 0.4
    so = TONGUE_CLR
    si = TONGUE_CLR + TONGUE_T
    g.add((0, W, 0, H, z0, z1), lambda X, Y, Z: np.maximum(
        ext(rr2(X, Y, CX, H / 2, W - 2 * WALL - 2 * so, H - 2 * WALL - 2 * so, R - WALL - so), dz_(Z, z0, z1)),
        -ext(rr2(X, Y, CX, H / 2, W - 2 * WALL - 2 * si, H - 2 * WALL - 2 * si, R - WALL - si), dz_(Z, z0 - 1, z1 + 1))))
    # solid collar where the lip joins the wall (keeps the mesh manifold at coarse voxel sizes)
    zc0, zc1 = SPLIT - 0.1, SPLIT + 0.8
    g.add((0, W, 0, H, zc0, zc1), lambda X, Y, Z: np.maximum(
        ext(rr2(X, Y, CX, H / 2, W - 2 * WALL + 0.2, H - 2 * WALL + 0.2, R - WALL + 0.1), dz_(Z, zc0, zc1)),
        -ext(rr2(X, Y, CX, H / 2, W - 2 * WALL - 2 * si, H - 2 * WALL - 2 * si, R - WALL - si), dz_(Z, zc0 - 1, zc1 + 1))))
    # screw columns (countersunk from the back)
    for (bx, by) in CLOSE_BOSS:
        g.addcylz(bx, by, BOSS_R, SPLIT, T - WALL + 0.5)
        g.cutcylz(bx, by, SCREW_CLR / 2, SPLIT - 0.5, T + 1)
        g.cutcylz(bx, by, 2.1, T - WALL + 0.8, T + 1)       # head pocket
    # ESP32 hold-down ribs (act on foam pad)
    for ry in (68.0, 85.0):
        g.addbox(20.0, 44.0, ry - 0.8, ry + 0.8, 13.9, T - WALL + 0.5)
    # LiPo cradle: four corner brackets
    bx0, bx1 = MOD["lipo_603450"][0] - 0.4, MOD["lipo_603450"][1] + 0.4
    by0, by1 = MOD["lipo_603450"][2] - 0.4, MOD["lipo_603450"][3] + 0.4
    zt, zb = T - WALL + 0.5, MOD["lipo_603450"][4] - 0.1
    t_, L_ = 1.2, 8.0
    for (cx_, sx) in ((bx0, -1), (bx1, 1)):
        for (cy_, sy) in ((by0, -1), (by1, 1)):
            xa, xb = sorted((cx_, cx_ + sx * L_))
            g.addbox(xa if sx > 0 else xa, xb, min(cy_, cy_ + sy * t_) if True else 0, max(cy_, cy_ + sy * t_), zb, zt)
            ya, yb = sorted((cy_, cy_ + sy * L_))
            g.addbox(min(cx_, cx_ + sx * t_), max(cx_, cx_ + sx * t_), ya, yb, zb, zt)
    lip_notches(g)
    connector_cuts(g)
    # engraved text (reads correctly when looking at the BACK of the device)
    g.cut_text("BMO PLAYER", CX, 62.0, 8.0, 62.0, T - 0.6, T + 1.0, flip_x=True)
    g.cut_text("@frandorturtles", CX, 46.0, 6.0, 62.0, T - 0.6, T + 1.0, flip_x=True)
    g.cut_text("portable mp3", CX, 34.0, 3.6, 40.0, T - 0.6, T + 1.0, flip_x=True)
    return g


def build_tray(h=H_SMALL):
    g = G((0, W, 0, 60, 3.0, TRAY_Z1 + 0.5), h)
    ymax = 56.6
    g.add((2, 74, 2, ymax, TRAY_Z0, TRAY_Z1), lambda X, Y, Z:
        ext(np.maximum(rr2(X, Y, CX, H / 2, W - 2 * WALL - 0.8, H - 2 * WALL - 0.8, R - WALL - 0.4), Y - ymax), dz_(Z, TRAY_Z0, TRAY_Z1)))
    for (bx, by) in CLOSE_BOSS[:2]:
        g.cutcylz(bx, by, BOSS_R + 0.9, TRAY_Z0 - 1, TRAY_Z1 + 1)
    for (bx, by) in TRAY_BOSS:
        g.cutcylz(bx, by, SCREW_CLR / 2, TRAY_Z0 - 1, TRAY_Z1 + 1)
    # switch pockets (rings in front of the plate) + pin holes
    for name, ((kx, ky), kind) in KEYS.items():
        ring_o, ring_i = SW_BODY + 2.0, SW_BODY + 0.3
        ry_o, ry_i = SW_PIN[1] + 3.1, SW_PIN[1] + 1.3          # legs stick out in Y
        g.addbox(kx - ring_o / 2, kx + ring_o / 2, ky - ry_o / 2, ky + ry_o / 2, 3.9, TRAY_Z0 + 0.3)
        g.cutbox(kx - ring_i / 2, kx + ring_i / 2, ky - ry_i / 2, ky + ry_i / 2, 3.8, TRAY_Z0)
        for sx in (-SW_PIN[0] / 2, SW_PIN[0] / 2):
            for sy in (-SW_PIN[1] / 2, SW_PIN[1] / 2):
                g.cutcylz(kx + sx, ky + sy, 0.7, TRAY_Z0 - 1, TRAY_Z1 + 1)
    return g


def frame_geom(name):
    x0, x1, y0, y1, z0, z1 = MOD[name]
    o = FRAME_CLR
    return (x0 - o, x1 + o, y0 - o, y1 + o)


def build_frames(h=H_SMALL):
    g = G((0, W, 0, 60, TRAY_Z1 - 0.5, TRAY_Z1 + FRAME_H + 0.5), h)
    z0, z1 = TRAY_Z1, TRAY_Z1 + FRAME_H
    for name, side in FRAME_MODS.items():
        ax0, ax1, ay0, ay1 = frame_geom(name)
        wl = FRAME_WALL
        # three/four walls
        if side != "-x":
            g.addbox(ax0 - wl, ax0, ay0 - (0 if side == "-y" else wl), ay1 + (0 if side == "+y" else wl), z0, z1)
        if side != "+x":
            g.addbox(ax1, ax1 + wl, ay0 - (0 if side == "-y" else wl), ay1 + (0 if side == "+y" else wl), z0, z1)
        if side != "-y":
            g.addbox(ax0, ax1, ay0 - wl, ay0, z0, z1)
        if side != "+y":
            g.addbox(ax0, ax1, ay1, ay1 + wl, z0, z1)
        # side-hold windows for the "open" side keep the connector free automatically
    return g


def build_key(kind, h=H_SMALL, arrow=None):
    """cap printed top-face-down: z'=0 is the visible face"""
    zs = CAP_PROTRUDE + WALL
    g = G((-9, 9, -9, 9, -0.5, zs + FLANGE_T + 0.5), h)
    if kind == "sq":
        g.addrrz(0, 0, SQ_SHAFT, SQ_SHAFT, 0.5, 0.0, zs + 0.1)
        g.addrrz(0, 0, SQ_FLANGE, SQ_FLANGE, 0.5, zs, zs + FLANGE_T)
        if arrow:
            a = {"up": 0, "left": 90, "down": 180, "right": 270}[arrow]
            c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
            g.cut((-5, 5, -5, 5, -1, 0.5),
                  lambda X, Y, Z: ext(tri2(c * X + s * Y, -s * X + c * Y, 0, 0.0, 1.5), dz_(Z, -1, 0.4)))
    elif kind == "gr":
        g.addcylz(0, 0, GR_SHAFT / 2, 0.0, zs + 0.1)
        g.addcylz(0, 0, GR_FLANGE / 2, zs, zs + FLANGE_T)
    elif kind == "rd":
        g.addcylz(0, 0, RD_SHAFT / 2, 0.0, zs + 0.1)
        g.addcylz(0, 0, RD_FLANGE / 2, zs, zs + FLANGE_T)
    else:
        g.add((-9, 9, -9, 9, 0, zs + 0.1), lambda X, Y, Z: ext(tri2(X, Y, 0, 0, TRI_RIN_SHAFT), dz_(Z, 0, zs + 0.1)))
        g.addcylz(0, 0, TRI_R_FLANGE, zs, zs + FLANGE_T)
    return g


def place_key_global(kind, name, h=H_SMALL):
    """cap in GLOBAL frame (for the fit check only)"""
    (kx, ky), _ = KEYS[name]
    g = G((kx - 9, kx + 9, ky - 9, ky + 9, -CAP_PROTRUDE - 0.5, WALL + FLANGE_T + 0.5), h)
    z0 = -CAP_PROTRUDE
    if kind == "sq":
        g.addrrz(kx, ky, SQ_SHAFT, SQ_SHAFT, 0.5, z0, WALL + 0.1)
        g.addrrz(kx, ky, SQ_FLANGE, SQ_FLANGE, 0.5, WALL, WALL + FLANGE_T)
    elif kind == "gr":
        g.addcylz(kx, ky, GR_SHAFT / 2, z0, WALL + 0.1)
        g.addcylz(kx, ky, GR_FLANGE / 2, WALL, WALL + FLANGE_T)
    elif kind == "rd":
        g.addcylz(kx, ky, RD_SHAFT / 2, z0, WALL + 0.1)
        g.addcylz(kx, ky, RD_FLANGE / 2, WALL, WALL + FLANGE_T)
    else:
        g.add((kx - 9, kx + 9, ky - 9, ky + 9, z0, WALL + 0.1),
              lambda X, Y, Z: ext(tri2(X, Y, kx, ky, TRI_RIN_SHAFT), dz_(Z, z0, WALL + 0.1)))
        g.addcylz(kx, ky, TRI_R_FLANGE, WALL, WALL + FLANGE_T)
    return g


# ----------------------------------------------------------------------------
# fit check
# ----------------------------------------------------------------------------
def overlap_mm3(grid, box_, shrink=0.2, thr=-0.05):
    x0, x1, y0, y1, z0, z1 = box_
    r = grid.reg((x0 + shrink, x1 - shrink, y0 + shrink, y1 - shrink, z0 + shrink, z1 - shrink), pad=0.0)
    if r is None:
        return 0.0
    sl, X, Y, Z = r
    inside = (X >= x0 + shrink) & (X <= x1 - shrink) & (Y >= y0 + shrink) & (Y <= y1 - shrink) & \
             (Z >= z0 + shrink) & (Z <= z1 - shrink)
    return float(np.count_nonzero((grid.d[sl] < thr) & inside)) * grid.h ** 3


def grid_overlap_mm3(a, b, thr=-0.15):
    """volume (mm3) where both grids are solid; samples grid a on its own nodes, looks up b"""
    ia, ja, ka = np.nonzero(a.d < thr)
    if len(ia) == 0:
        return 0.0
    x, y, z = a.xs[ia], a.ys[ja], a.zs[ka]
    ib = np.rint((x - b.x0) / b.h).astype(int)
    jb = np.rint((y - b.y0) / b.h).astype(int)
    kb = np.rint((z - b.z0) / b.h).astype(int)
    ok = (ib >= 0) & (ib < b.nx) & (jb >= 0) & (jb < b.ny) & (kb >= 0) & (kb < b.nz)
    hit = np.zeros(len(ia), bool)
    hit[ok] = b.d[ib[ok], jb[ok], kb[ok]] < thr
    return float(hit.sum()) * a.h ** 3


def box_overlap(a, b):
    dx = min(a[1], b[1]) - max(a[0], b[0])
    dy = min(a[3], b[3]) - max(a[2], b[2])
    dz = min(a[5], b[5]) - max(a[4], b[4])
    return max(dx, 0) * max(dy, 0) * max(dz, 0) if min(dx, dy, dz) > 0.01 else 0.0


def height_render(g, view, path, scale=4):
    """quick shaded height-map render (orthographic) straight from the voxel model"""
    d = g.d < 0
    if view == "front":
        idx = np.argmax(d, axis=2)
        has = d.any(axis=2)
        hz = g.zs[idx]
    else:
        idx = d.shape[2] - 1 - np.argmax(d[:, :, ::-1], axis=2)
        has = d.any(axis=2)
        hz = -g.zs[idx]
    hz = np.where(has, hz, hz[has].min() - 4 if has.any() else 0)
    gx, gy = np.gradient(hz, g.h)
    shade = np.clip(0.62 + 0.9 * (-gx * 0.5 + gy * 0.6) / (1 + np.hypot(gx, gy)), 0.15, 1.0)
    img = np.where(has[:, :, None], np.clip(shade[:, :, None] * np.array([[[150, 235, 215]]]), 0, 255), 255).astype(np.uint8)
    arr = np.transpose(img, (1, 0, 2))[::-1]          # image rows = y descending
    if view == "back":
        arr = arr[:, ::-1]                              # seen from the back: x mirrored
    im = Image.fromarray(arr)
    im = im.resize((im.width * scale // 3, im.height * scale // 3), Image.LANCZOS)
    im.save(path)


def main():
    os.makedirs(OUT_STL, exist_ok=True)
    os.makedirs(OUT_REN, exist_ok=True)
    os.makedirs(OUT_TEST, exist_ok=True)
    report = {"parts": [], "fit": {}, "params": {"W": W, "H": H, "T": T, "wall": WALL, "voxel_shell_mm": H_SHELL,
                                                  "voxel_small_mm": H_SMALL}}
    print("front shell ...", flush=True)
    front = build_front()
    report["parts"].append(write_stl(os.path.join(OUT_STL, "bmo_front_shell.stl"), *front.mesh()))
    height_render(front, "front", os.path.join(OUT_REN, "front_shell_top_view.png"))
    print("back shell ...", flush=True)
    back = build_back()
    report["parts"].append(write_stl(os.path.join(OUT_STL, "bmo_back_shell.stl"), *back.mesh(),
                                     xf=lambda v: np.stack([W - v[:, 0], v[:, 1], T - v[:, 2]], 1)))
    height_render(back, "back", os.path.join(OUT_REN, "back_outside_view.png"))
    print("tray / frames / keys ...", flush=True)
    tray = build_tray()
    report["parts"].append(write_stl(os.path.join(OUT_STL, "bmo_switch_tray.stl"), *tray.mesh(),
                                     xf=lambda v: np.stack([W - v[:, 0], v[:, 1], TRAY_Z1 - v[:, 2]], 1)))
    frames = build_frames()
    report["parts"].append(write_stl(os.path.join(OUT_STL, "bmo_module_frames.stl"), *frames.mesh(),
                                     xf=lambda v: v - np.array([0, 0, TRAY_Z1])))
    for name, ((kx, ky), kind) in KEYS.items():
        kg = build_key(kind, arrow=name if kind == "sq" else None)
        report["parts"].append(write_stl(os.path.join(OUT_STL, f"bmo_key_{name}.stl"), *kg.mesh()))

    print("fit check ...", flush=True)
    caps = {n: place_key_global(k, n) for n, (_, k) in KEYS.items()}
    solids = {"front_shell": front, "back_shell": back, "switch_tray": tray, "module_frames": frames}
    fit = {"module_vs_solid_mm3": {}, "module_vs_module_mm3": {}, "part_vs_part_mm3": {}, "inside_envelope": {}}
    for mname, mb in MOD.items():
        for sname, sg in solids.items():
            v = overlap_mm3(sg, mb)
            if v > 0.5:
                fit["module_vs_solid_mm3"][f"{mname} x {sname}"] = round(v, 1)
        for cname, cg in caps.items():
            v = overlap_mm3(cg, mb)
            if v > 0.5:
                fit["module_vs_solid_mm3"][f"{mname} x key_{cname}"] = round(v, 1)
        x0, x1, y0, y1, z0, z1 = mb
        fit["inside_envelope"][mname] = bool(x0 >= WALL - 0.01 and x1 <= W - WALL + 0.01 and y0 >= WALL - 0.01
                                             and y1 <= H - WALL + 0.01 and z0 >= WALL - 0.01 and z1 <= T - WALL + 0.01)
    names = list(MOD)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            v = box_overlap(MOD[names[i]], MOD[names[j]])
            if v > 0:
                fit["module_vs_module_mm3"][f"{names[i]} x {names[j]}"] = round(v, 1)
    for cname, cg in caps.items():
        v = grid_overlap_mm3(cg, front)
        if v > 0.5:
            fit["part_vs_part_mm3"][f"key_{cname} x front_shell"] = round(v, 1)
        v = grid_overlap_mm3(cg, tray)
        if v > 0.5:
            fit["part_vs_part_mm3"][f"key_{cname} x tray"] = round(v, 1)
    for a, b in (("front_shell", "switch_tray"), ("front_shell", "back_shell"), ("back_shell", "switch_tray"),
                 ("module_frames", "switch_tray"), ("module_frames", "front_shell")):
        v = grid_overlap_mm3(solids[a], solids[b])
        if v > 0.5:
            fit["part_vs_part_mm3"][f"{a} x {b}"] = round(v, 1)
    report["fit"] = fit
    report["modules_nominal_mm"] = {k: [round(v[1] - v[0], 2), round(v[3] - v[2], 2), round(v[5] - v[4], 2)]
                                    for k, v in MOD.items()}
    with open(os.path.join(OUT_TEST, "fit_report.json"), "w") as fh:
        json.dump(report, fh, indent=2)
    print(json.dumps(report["fit"], indent=2))
    for p in report["parts"]:
        print(p)


if __name__ == "__main__":
    main()
