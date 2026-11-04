#!/usr/bin/env python3
"""Quick-and-dirty Gerber/drill rasterizer for visual inspection."""
import re, math
from PIL import Image, ImageDraw

SCALE = 20  # px per mm

class Gerber:
    def __init__(self, path, fmt=5):
        self.path = path
        self.fmt = fmt
        self.apertures = {}
        self.macros = {}
        self.ops = []  # ('line', x1,y1,x2,y2,w) ('flash', x,y,shape,params) ('poly', pts)
        self.parse()

    def parse(self):
        cur = None
        x = y = 0.0
        region = None
        lastop = None
        for raw in open(self.path):
            line = raw.strip()
            if not line:
                continue
            m = re.match(r"%AM([^*]+)\*(.*)\*%", line)
            if m:
                name, body = m.groups()
                prims = []
                for p in body.split("*"):
                    if p.strip():
                        prims.append([float(v) for v in p.split(",")])
                self.macros[name] = prims
                continue
            m = re.match(r"%ADD(\d+)([A-Za-z]+),?([0-9.Xx]*)\*%", line)
            if m:
                code, shape, dims = m.groups()
                params = [float(v) for v in re.split("[Xx]", dims)] if dims else []
                self.apertures[code] = (shape, params)
                continue
            if line.startswith("%"):
                continue
            if line == "G36*":
                region = []
                continue
            if line == "G37*":
                if region:
                    self.ops.append(("poly", region))
                region = None
                continue
            m = re.match(r"D(\d+)\*$", line)
            if m:
                cur = m.group(1)
                continue
            m = re.match(r"(?:G0([123]))?X(-?\d+)Y(-?\d+)(?:D0([123]))?\*", line)
            if not m:
                m2 = re.match(r"(?:G0([123]))?X(-?\d+)Y(-?\d+)I(-?\d+)J(-?\d+)D0([123])\*", line)
                if m2:
                    g, xs, ys, is_, js, op = m2.groups()
                    nx, ny = int(xs) / 10**self.fmt, int(ys) / 10**self.fmt
                    cx, cy = x + int(is_) / 10**self.fmt, y + int(js) / 10**self.fmt
                    self.arc(x, y, nx, ny, cx, cy, g == "2")
                    x, y = nx, ny
                continue
            g, xs, ys, op = m.groups()
            nx, ny = int(xs) / 10**self.fmt, int(ys) / 10**self.fmt
            if op == "1":
                if region is not None:
                    region.append((nx, ny))
                else:
                    w = self.apertures.get(cur, ("C", [0.1]))[1][0]
                    self.ops.append(("line", x, y, nx, ny, w))
            elif op == "3":
                shape, params = self.apertures.get(cur, ("C", [0.1]))
                self.ops.append(("flash", nx, ny, shape, params))
            x, y = nx, ny

    def arc(self, x1, y1, x2, y2, cx, cy, cw):
        r = math.hypot(x1 - cx, y1 - cy)
        a1 = math.atan2(y1 - cy, x1 - cx)
        a2 = math.atan2(y2 - cy, x2 - cx)
        if cw:
            while a2 >= a1: a2 -= 2 * math.pi
        else:
            while a2 <= a1: a2 += 2 * math.pi
        steps = max(4, int(abs(a2 - a1) * r * 4))
        pts = []
        for i in range(steps + 1):
            a = a1 + (a2 - a1) * i / steps
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
        w = 0.15
        for i in range(len(pts) - 1):
            self.ops.append(("line", pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], w))

def draw_ops(draw, ops, color, scale=SCALE, ymax=64.0, MACROS=None):
    def P(x, y): return (x * scale, (-y) * scale)
    for op in ops:
        if op[0] == "line":
            _, x1, y1, x2, y2, w = op
            draw.line([P(x1, y1), P(x2, y2)], fill=color, width=max(1, int(w * scale)))
            r = max(1, w * scale / 2)
            for (x, y) in [(x1, y1), (x2, y2)]:
                px, py = P(x, y)
                draw.ellipse([px - r, py - r, px + r, py + r], fill=color)
        elif op[0] == "poly":
            pts = [P(x, y) for x, y in op[1]]
            if len(pts) >= 3:
                draw.polygon(pts, fill=color)
        elif op[0] == "flash":
            _, x, y, shape, params = op
            px, py = P(x, y)
            if shape == "C":
                r = params[0] * scale / 2
                draw.ellipse([px - r, py - r, px + r, py + r], fill=color)
            elif shape == "R":
                w, h = params[0] * scale, params[1] * scale
                draw.rectangle([px - w / 2, py - h / 2, px + w / 2, py + h / 2], fill=color)
            elif shape == "O":
                w, h = params[0] * scale, params[1] * scale
                draw.rounded_rectangle([px - w / 2, py - h / 2, px + w / 2, py + h / 2], radius=min(w, h) / 2, fill=color)
            elif shape.startswith("MACRO"):
                for prim in MACROS.get(shape, []):
                    if prim[0] == 4:
                        n = int(prim[2])
                        coords = prim[3:3 + 2 * n]
                        pts = [(px + coords[i] * scale, py + coords[i + 1] * scale) for i in range(0, len(coords), 2)]
                        if len(pts) >= 3:
                            draw.polygon(pts, fill=color)

def render_board(out, layers):
    W, H = int(75 * SCALE), int(64 * SCALE)
    im = Image.new("RGB", (W, H), "white")
    d = ImageDraw.Draw(im, "RGBA")
    for path, color, fmt in layers:
        g = Gerber(path, fmt)
        draw_ops(d, g.ops, color, MACROS=g.macros)
    im.save(out)
    print("wrote", out)

if __name__ == "__main__":
    from pathlib import Path
    base = Path(__file__).resolve().parent.parent / "pcb"
    render_board("/tmp/opencode/board.png", [
        (base / "Gerber_TopLayer.GTL", (220, 0, 0, 120), 5),
        (base / "Gerber_TopSolderMaskLayer.GTS", (255, 180, 0, 200), 5),
        (base / "Gerber_TopSilkscreenLayer.GTO", (0, 0, 0, 255), 5),
        (base / "Gerber_BoardOutlineLayer.GKO", (0, 0, 255, 255), 5),
    ])
    render_board("/tmp/opencode/board_bottom.png", [
        (base / "Gerber_BottomLayer.GBL", (0, 100, 220, 120), 5),
        (base / "Gerber_BottomSolderMaskLayer.GBS", (255, 180, 0, 200), 5),
        (base / "Gerber_BottomSilkscreenLayer.GBO", (0, 0, 0, 255), 5),
        (base / "Gerber_BoardOutlineLayer.GKO", (0, 0, 255, 255), 5),
    ])
