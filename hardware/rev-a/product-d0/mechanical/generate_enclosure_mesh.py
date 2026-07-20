#!/usr/bin/env python3
"""Generate dependency-free D0 enclosure envelope STLs.

These meshes verify orientation, product volume, display/control placement, and the
JLCEDA shell-import path. They intentionally use overlapping watertight cuboids and
are not production printable shells.
"""

from pathlib import Path

OUT = Path(__file__).resolve().parent / "generated"


def tri(a, b, c):
    ux, uy, uz = (b[i] - a[i] for i in range(3))
    vx, vy, vz = (c[i] - a[i] for i in range(3))
    nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
    return (nx, ny, nz), a, b, c


def cuboid(cx, cy, z0, sx, sy, sz):
    x0, x1, y0, y1, z1 = cx-sx/2, cx+sx/2, cy-sy/2, cy+sy/2, z0+sz
    v=[(x0,y0,z0),(x1,y0,z0),(x1,y1,z0),(x0,y1,z0),(x0,y0,z1),(x1,y0,z1),(x1,y1,z1),(x0,y1,z1)]
    q=[(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)]
    return [tri(v[a],v[b],v[c]) for a,b,c in q]


def write(path, solids):
    with path.open("w") as f:
        f.write("solid cadenza_d0\n")
        for normal,a,b,c in [t for s in solids for t in s]:
            f.write(f" facet normal {normal[0]} {normal[1]} {normal[2]}\n  outer loop\n")
            for p in (a,b,c): f.write(f"   vertex {p[0]} {p[1]} {p[2]}\n")
            f.write("  endloop\n endfacet\n")
        f.write("endsolid cadenza_d0\n")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    # Front bezel: 2 mm plate represented by bars around the display opening.
    front=[
        cuboid(0, 28.0, 0, 122, 18, 2), cuboid(0, -28.0, 0, 122, 18, 2),
        cuboid(-46.0, 0, 0, 30, 38, 2), cuboid(46.0, 0, 0, 30, 38, 2),
        # Rim walls and display support rails.
        cuboid(0, 36, 0, 106, 2, 9.5), cuboid(0, -36, 0, 106, 2, 9.5),
        cuboid(-60, 0, 0, 2, 56, 9.5), cuboid(60, 0, 0, 2, 56, 9.5),
        cuboid(0, 19.8, 1.8, 63.2, 1.6, 2.0), cuboid(0, -19.8, 1.8, 63.2, 1.6, 2.0),
    ]
    # Rear: shallow plate plus two rails that express the two desk-rest facets.
    rear=[
        cuboid(0,0,9.5,122,74,2.0),
        cuboid(-25,-30,11.5,42,8,5.5),
        cuboid(25,-27,11.5,34,14,5.5),
        cuboid(-48,0,11.5,12,50,5.5), cuboid(48,0,11.5,12,50,5.5),
    ]
    write(OUT/"cadenza-front-envelope-d0.stl",front)
    write(OUT/"cadenza-rear-envelope-d0.stl",rear)
    write(OUT/"cadenza-assembled-envelope-d0.stl",front+rear)


if __name__ == "__main__": main()

