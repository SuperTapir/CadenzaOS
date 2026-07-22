#!/usr/bin/env python3
import importlib.util,json,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
if importlib.util.find_spec("OCP") is None:
 sys.path.insert(0,"/Users/tapir/.cache/cadenza-cad-tools-py313")
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib
from OCP.BRepCheck import BRepCheck_Analyzer
from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPControl import STEPControl_Reader
def valid(p):
 r=STEPControl_Reader();
 if r.ReadFile(str(p))!=IFSelect_RetDone:return False
 r.TransferRoots();s=r.OneShape();return not s.IsNull() and BRepCheck_Analyzer(s).IsValid()
def bbox(p):
 r=STEPControl_Reader();assert r.ReadFile(str(p))==IFSelect_RetDone;r.TransferRoots();b=Bnd_Box();BRepBndLib.Add_s(r.OneShape(),b);return b.Get()
def spans(p):
 x1,y1,z1,x2,y2,z2=bbox(p);return [round(x2-x1,3),round(y2-y1,3),round(z2-z1,3)]
def centre(p):
 x1,y1,z1,x2,y2,z2=bbox(p);return [(x1+x2)/2,(y1+y2)/2,(z1+z2)/2]
def main():
 p=HERE/"generated/power-lock-s1-fit.json";d=json.loads(p.read_text());checks={"status_exact":d["status"]=="candidate only; selection_frozen=false; production_ready=false","axis_semantics":d["axis_semantics"]=={"local_X":"width","local_Y":"front-back; +Y outward; travel along Y","local_Z":"height above PCB","H035_axis":"Y"},"body_xyz_exact":d["body_nominal_xyz_mm"]==[4.6,2.3,1.8],"H_is_Y_not_Z":d["overall_Y_H035_mm"]==3.5 and d["actuator"]["centre_height_CH_Z_mm"]==0.95,"travel_exact":d["travel_mm"]=={"axis":"Y","nominal":0.2,"tolerance":0.1,"checked_worst_case":0.3},"internal_datums_separate":d["side_view_internal_Y_datums_not_external_projection_mm"]==[0.66,0.85,2.05],"variant_set":set(d["variants"])=={"top-edge-A","right-edge-B"},"directions":d["variants"]["top-edge-A"]["press_global"]=="-Y" and d["variants"]["right-edge-B"]["press_global"]=="-X","collisions_zero":all(abs(x)<=1e-6 for v in d["variants"].values() for x in v["hard_collision_common_volume_mm3"].values())}
 steps=[]
 for name in d["variants"]:
  for part in ("body","actuator-rest","actuator-pressed-nominal","actuation-sweep-0p3","assembly"):
   for ext in ("step","stl"):
    f=HERE/"generated"/name/f"{name}-{part}.{ext}";checks[f"exists:{name}:{part}:{ext}"]=f.is_file() and f.stat().st_size>100
   steps.append(HERE/"generated"/name/f"{name}-{part}.step")
 top=HERE/"generated/top-edge-A";right=HERE/"generated/right-edge-B"
 checks["top_body_axis_spans"]=spans(top/"top-edge-A-body.step")==[4.6,2.3,1.8]
 checks["right_body_rotated_axis_spans"]=spans(right/"right-edge-B-body.step")==[2.3,4.6,1.8]
 tc0=centre(top/"top-edge-A-actuator-rest.step");tc1=centre(top/"top-edge-A-actuator-pressed-nominal.step")
 rc0=centre(right/"right-edge-B-actuator-rest.step");rc1=centre(right/"right-edge-B-actuator-pressed-nominal.step")
 checks["top_press_translation_minus_Y"]=abs(tc1[0]-tc0[0])<1e-6 and abs((tc1[1]-tc0[1])+0.2)<1e-6 and abs(tc1[2]-tc0[2])<1e-6
 checks["right_press_translation_minus_X"]=abs((rc1[0]-rc0[0])+0.2)<1e-6 and abs(rc1[1]-rc0[1])<1e-6 and abs(rc1[2]-rc0[2])<1e-6
 checks["all_steps_valid"]=all(valid(x) for x in steps);result={"status":"PASS" if all(checks.values()) else "FAIL","selection_frozen":False,"production_ready":False,"valid_step_count":sum(valid(x) for x in steps),"checks":checks}
 (HERE/"verification.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n");print(json.dumps(result,ensure_ascii=False));return 0 if result["status"]=="PASS" else 1
if __name__=="__main__":raise SystemExit(main())
