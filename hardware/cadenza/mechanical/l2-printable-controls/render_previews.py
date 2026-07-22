#!/usr/bin/env python3
"""Render dependency-free orthographic SVG previews from generated ASCII STL."""

from __future__ import annotations

import math
from pathlib import Path


HERE = Path(__file__).resolve().parent
CASES = {
    "b-keycap": HERE / "generated/common/cadenza-b-keycap-candidate.stl",
    "menu-keycap": HERE / "generated/common/cadenza-menu-keycap-candidate.stl",
    "a-knob-round": HERE / "generated/a-balanced/cadenza-a-knob-round-candidate.stl",
    "knurled-adapter-insert": HERE / "generated/common/cadenza-knurled-adapter-insert-candidate.stl",
}


def triangles(path):
    result, current = [], []
    with path.open(encoding="ascii") as stream:
        for line in stream:
            fields = line.split()
            if fields[:1] == ["vertex"]:
                current.append(tuple(float(v) for v in fields[1:4]))
                if len(current) == 3:
                    result.append(tuple(current))
                    current = []
    return result


def project(point, azimuth=-35, elevation=58):
    x, y, z = point
    az, el = math.radians(azimuth), math.radians(elevation)
    x1 = math.cos(az) * x - math.sin(az) * y
    y1 = math.sin(az) * x + math.cos(az) * y
    return x1, math.cos(el) * y1 - math.sin(el) * z, math.sin(el) * y1 + math.cos(el) * z


def render(source, target, width=900, height=650):
    projected = [tuple(project(p) for p in tri) for tri in triangles(source)]
    xs = [p[0] for tri in projected for p in tri]
    ys = [p[1] for tri in projected for p in tri]
    margin = 35
    scale = min((width - 2 * margin) / (max(xs) - min(xs)), (height - 2 * margin) / (max(ys) - min(ys)))
    projected.sort(key=lambda tri: sum(p[2] for p in tri) / 3)
    polygons = []
    for tri in projected:
        a, b, c = tri
        ab = tuple(b[i] - a[i] for i in range(3))
        ac = tuple(c[i] - a[i] for i in range(3))
        normal = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        length = math.sqrt(sum(v * v for v in normal)) or 1
        light = (-0.25, -0.35, 0.90)
        shade = 0.35 + 0.55 * abs(sum(normal[i] / length * light[i] for i in range(3)))
        rgb = tuple(int(v * shade) for v in (91, 139, 174))
        points = " ".join(
            f"{margin + (p[0] - min(xs)) * scale:.2f},{height - margin - (p[1] - min(ys)) * scale:.2f}"
            for p in tri
        )
        polygons.append(f'<polygon points="{points}" fill="rgb{rgb}" stroke="#293941" stroke-width="0.35"/>')
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
<rect width="100%" height="100%" fill="#f6f5f0"/>
<text x="18" y="26" font-family="sans-serif" font-size="16">{target.stem} — STL visual audit only / NOT FROZEN</text>
{''.join(polygons)}
</svg>\n'''
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(svg, encoding="utf-8")


def main():
    for name, source in CASES.items():
        render(source, HERE / "render" / f"{name}.svg")
    print({"rendered": len(CASES), "format": "svg", "output": str(HERE / 'render')})


if __name__ == "__main__":
    main()
