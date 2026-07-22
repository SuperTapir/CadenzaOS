#!/usr/bin/env python3
"""Generate a simple datasheet-dimensioned STEP/STL envelope."""
from pathlib import Path
import importlib.util
import sys

HERE = Path(__file__).resolve().parent
if importlib.util.find_spec("OCP") is None:
    sys.path.insert(0, "/Users/tapir/.cache/cadenza-cad-tools-py313")
from OCP.BRep import BRep_Builder
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox
from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPControl import STEPControl_AsIs, STEPControl_Writer
from OCP.StlAPI import StlAPI_Writer
from OCP.TopoDS import TopoDS_Compound
from OCP.gp import gp_Pnt


def box(x1, y1, z1, x2, y2, z2):
    return BRepPrimAPI_MakeBox(gp_Pnt(x1, y1, z1), x2-x1, y2-y1, z2-z1).Shape()


def main():
    # Local axes: X=width, Y=front/back and edge-outward, Z=above PCB.
    # PDF p.1: body X/Y/Z=4.6/2.3/1.8; H035=3.50 is total Y,
    # not Z height.  The actuator is 1.85 wide and 0.85 high, centred at
    # CH=0.95 above PCB.  The simple envelope shares the rear body datum.
    body = box(-2.3, -1.15, 0, 2.3, 1.15, 1.8)
    stem = box(-0.925, 1.15, 0.525, 0.925, 2.35, 1.375)
    result = TopoDS_Compound(); builder = BRep_Builder(); builder.MakeCompound(result)
    builder.Add(result, body); builder.Add(result, stem)
    out = HERE / "3dmodels"; out.mkdir(parents=True, exist_ok=True)
    step_path = out / "GT-TC020A-H035-L1-envelope.step"
    writer = STEPControl_Writer(); writer.Transfer(result, STEPControl_AsIs)
    if writer.Write(str(step_path)) != IFSelect_RetDone: raise RuntimeError("STEP write failed")
    BRepMesh_IncrementalMesh(result, 0.03, False, 0.25, True)
    if not StlAPI_Writer().Write(result, str(out / "GT-TC020A-H035-L1-envelope.stl")):
        raise RuntimeError("STL write failed")
    return 0


if __name__ == "__main__": raise SystemExit(main())
