#!/usr/bin/env python3
"""Minimal Gerber -> PostScript renderer for visual inspection (no arcs)."""
import re, sys

def render(path, out, color=(0, 0, 0), extra_scale=1.0, only_strokes=True):
    apertures = {}
    cur = None
    x = y = 0.0
    lines = []
    region = False
    for raw in open(path):
        line = raw.strip()
        m = re.match(r"%ADD(\d+)([A-Z]),([0-9.X]+)\*%", line)
        if m:
            code, shape, dims = m.groups()
            apertures[code] = (shape, float(dims.split("X")[0]))
            continue
        if line == "%LPD*%" or line == "%LPC*%":
            continue
        if line == "G36*":
            region = True
            continue
        if line == "G37*":
            region = False
            continue
        m = re.match(r"D(\d+)\*$", line)
        if m and m.group(1) != "01" and m.group(1) != "02" and m.group(1) != "03":
            cur = m.group(1)
            continue
        m = re.match(r"X(-?\d+)Y(-?\d+)D0([123])\*", line)
        if m:
            nx, ny = int(m.group(1)) / 1e5, int(m.group(2)) / 1e5
            op = m.group(3)
            if op == "1" and not region:
                w = apertures.get(cur, ("C", 0.1))[1]
                lines.append((x, y, nx, ny, w))
            x, y = nx, ny
    # PostScript: 1pt = 1/72in, 1mm = 72/25.4pt; flip Y
    s = 72 / 25.4 * extra_scale
    H = 64.0
    with open(out, "w") as f:
        f.write("%!PS-Adobe-3.0 EPSF-3.0\n")
        f.write(f"%%BoundingBox: 0 0 {75*s:.2f} {H*s:.2f}\n")
        f.write(f"{color[0]} {color[1]} {color[2]} setrgbcolor 1 setlinecap 1 setlinejoin\n")
        for (x1, y1, x2, y2, w) in lines:
            f.write(f"newpath {x1*s:.4f} {(H+y1)*s:.4f} moveto {x2*s:.4f} {(H+y2)*s:.4f} lineto {w*s:.4f} setlinewidth stroke\n")
        f.write("showpage\n")
    print(f"{path}: {len(lines)} strokes -> {out}")

if __name__ == "__main__":
    render(sys.argv[1], sys.argv[2])
