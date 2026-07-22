#!/usr/bin/env python3
"""Generate non-frozen GT-TC020A S1 fits at inherited A/B positions."""
from __future__ import annotations
import importlib.util, json, math, sys
from pathlib import Path

HERE=Path(__file__).resolve().parent; REPO=HERE.parents[3]
if importlib.util.find_spec("OCP") is None:
 sys.path.insert(0,"/Users/tapir/.cache/cadenza-cad-tools-py313")
from OCP.BRep import BRep_Builder
from OCP.BRepAlgoAPI import BRepAlgoAPI_Common
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.BRepGProp import BRepGProp
from OCP.BRepMesh import BRepMesh_IncrementalMesh
from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
from OCP.GProp import GProp_GProps
from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
from OCP.StlAPI import StlAPI_Writer
from OCP.TopoDS import TopoDS_Compound
from OCP.gp import gp_Ax2,gp_Dir,gp_Pnt

FRONT=REPO/"hardware/cadenza/mechanical/l1-fit-check/generated/l1-front-fitcheck.step"
PCB_Z=0.4; TRAVEL_NOM=0.2; TRAVEL_TOL=0.1
SCREEN=(33.6,8.59,96.4,51.41); FPC=(60.18,50.8,69.82,58.0)
HOLES=((5,5),(5,55),(125,5),(125,55)); L2=((19,29,5.25),(18,43,4),(112,30,9),(20,27,5.25),(20,39,4),(111,30,8),(18,31,5.25),(23,44,4),(112,32,9))
VARIANTS={
 "top-edge-A":{"origin_xy_mm":[105.0,58.85],"outward_global":"+Y","press_global":"-Y","rotation_deg":0},
 "right-edge-B":{"origin_xy_mm":[128.85,18.0],"outward_global":"+X","press_global":"-X","rotation_deg":-90},
}

def box(x1,y1,z1,x2,y2,z2): return BRepPrimAPI_MakeBox(gp_Pnt(x1,y1,z1),x2-x1,y2-y1,z2-z1).Shape()
def cyl(x,y,r): return BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(x,y,-5),gp_Dir(0,0,1)),r,15).Shape()
def compound(items):
 c=TopoDS_Compound(); b=BRep_Builder(); b.MakeCompound(c)
 for x in items:b.Add(c,x)
 return c
def read_step(p):
 r=STEPControl_Reader(); assert r.ReadFile(str(p))==IFSelect_RetDone; r.TransferRoots(); return r.OneShape()
def common(a,b):
 op=BRepAlgoAPI_Common(a,b);op.Build();pr=GProp_GProps();BRepGProp.VolumeProperties_s(op.Shape(),pr);return pr.Mass()
def write(shape,p):
 assert BRepCheck_Analyzer(shape).IsValid(); w=STEPControl_Writer();w.Transfer(shape,STEPControl_AsIs);assert w.Write(str(p.with_suffix('.step')))==IFSelect_RetDone
 BRepMesh_IncrementalMesh(shape,0.03,False,0.25,True);assert StlAPI_Writer().Write(shape,str(p.with_suffix('.stl')))

def local_to_global(name,x1,y1,x2,y2):
 ox,oy=VARIANTS[name]["origin_xy_mm"]
 if name=="top-edge-A": return ox+x1,oy+y1,ox+x2,oy+y2
 # local +Y maps to global +X; local +X maps to global -Y
 return ox+y1,oy-x2,ox+y2,oy-x1

def geom(name):
 # PDF p.1 axis semantics: X=width, Y=front/back/outward, Z=PCB height.
 # Body X/Y/Z=4.6/2.3/1.8. H035=3.5 is total Y, never Z.
 bx1,by1,bx2,by2=local_to_global(name,-2.3,-1.15,2.3,1.15)
 body=box(bx1,by1,PCB_Z,bx2,by2,PCB_Z+1.8)
 # Actuator: X width 1.85, Z height 0.85 centred at CH=0.95.  The
 # simple envelope runs from body front Y=1.15 to H tip Y=2.35.
 ax1,ay1,ax2,ay2=local_to_global(name,-0.925,1.15,0.925,2.35)
 rest=box(ax1,ay1,PCB_Z+0.525,ax2,ay2,PCB_Z+1.375)
 shift=(-TRAVEL_NOM if name=="right-edge-B" else 0,-TRAVEL_NOM if name=="top-edge-A" else 0)
 maxshift=(-(TRAVEL_NOM+TRAVEL_TOL) if name=="right-edge-B" else 0,-(TRAVEL_NOM+TRAVEL_TOL) if name=="top-edge-A" else 0)
 pressed=box(ax1+shift[0],ay1+shift[1],PCB_Z+0.525,ax2+shift[0],ay2+shift[1],PCB_Z+1.375)
 maxpressed=box(ax1+maxshift[0],ay1+maxshift[1],PCB_Z+0.525,ax2+maxshift[0],ay2+maxshift[1],PCB_Z+1.375)
 sweep=compound([rest,maxpressed]); return body,rest,pressed,sweep

def main():
 out=HERE/"generated";out.mkdir(parents=True,exist_ok=True);front=read_step(FRONT)
 screen=box(SCREEN[0],SCREEN[1],-5,SCREEN[2],SCREEN[3],10);fpc=box(FPC[0],FPC[1],-5,FPC[2],FPC[3],10);mounts=compound([cyl(x,y,5) for x,y in HOLES]);l2=compound([cyl(x,y,r) for x,y,r in L2])
 data={"schema_version":2,"status":"candidate only; selection_frozen=false; production_ready=false","source_pdf_page":1,"mpn":"GT-TC020A-H035-L1","axis_semantics":{"local_X":"width","local_Y":"front-back; +Y outward; travel along Y","local_Z":"height above PCB","H035_axis":"Y"},"body_nominal_xyz_mm":[4.6,2.3,1.8],"overall_Y_H035_mm":3.5,"actuator":{"width_X_mm":1.85,"height_Z_mm":0.85,"centre_height_CH_Z_mm":0.95,"simple_envelope_Y_mm":[1.15,2.35]},"side_view_internal_Y_datums_not_external_projection_mm":[0.66,0.85,2.05],"travel_mm":{"axis":"Y","nominal":0.2,"tolerance":0.1,"checked_worst_case":0.3},"variants":{},"limits":["No enclosure button cap/guide is selected.","No complete back shell or PCB component Z-stack is proven.","Physical operating force and feel remain to be tested.","0.66 and horizontal 0.85 are internal side-view datums, not travel or an external stem projection."]}
 for name in VARIANTS:
  folder=out/name;folder.mkdir(parents=True,exist_ok=True);body,rest,pressed,sweep=geom(name);assembly=compound([front,body,rest])
  for part,shape in (("body",body),("actuator-rest",rest),("actuator-pressed-nominal",pressed),("actuation-sweep-0p3",sweep),("assembly",assembly)):write(shape,folder/f"{name}-{part}")
  switch=compound([body,sweep]); collisions={"screen":common(screen,switch),"fpc":common(fpc,switch),"mount_keepouts":common(mounts,switch),"l2_controls":common(l2,switch),"front_shell":common(front,switch)}
  rec={**VARIANTS[name],"body_nominal_xyz_mm":[4.6,2.3,1.8],"overall_Y_H035_mm":3.5,"actuator_nominal_width_X_mm":1.85,"actuator_height_Z_mm":0.85,"actuator_centre_height_CH_Z_mm":0.95,"hard_collision_common_volume_mm3":collisions}
  data["variants"][name]=rec;(folder/f"{name}-parameters.json").write_text(json.dumps(rec,ensure_ascii=False,indent=2)+"\n")
 (out/"power-lock-s1-fit.json").write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n")
 print(json.dumps({"status":"pass","variants":2,"selection_frozen":False,"production_ready":False}));return 0
if __name__=="__main__":raise SystemExit(main())
