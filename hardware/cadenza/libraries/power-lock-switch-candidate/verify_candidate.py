#!/usr/bin/env python3
"""Verify source hash, exact footprint primitives and KiCad 10 parsing."""
import hashlib,importlib.util,json,re,subprocess,tempfile,sys
from pathlib import Path
HERE=Path(__file__).resolve().parent;REPO=HERE.parents[3]
if importlib.util.find_spec("OCP") is None:
 sys.path.insert(0,"/Users/tapir/.cache/cadenza-cad-tools-py313")
from OCP.Bnd import Bnd_Box
from OCP.BRepBndLib import BRepBndLib
from OCP.IFSelect import IFSelect_RetDone
from OCP.STEPControl import STEPControl_Reader
PDF=REPO/"hardware/cadenza/sourcing/power-lock-switch/datasheets/C17179533_GT-TC020A-H035-L1.pdf"
MOD=HERE/"GT_TC020A_H035_L1.pretty/GT-TC020A-H035-L1.kicad_mod"
KICAD=Path("/Applications/KiCad.app/Contents/MacOS/kicad-cli")
def bbox_span(p):
 r=STEPControl_Reader();assert r.ReadFile(str(p))==IFSelect_RetDone;r.TransferRoots();b=Bnd_Box();BRepBndLib.Add_s(r.OneShape(),b);x1,y1,z1,x2,y2,z2=b.Get();return [round(x2-x1,3),round(y2-y1,3),round(z2-z1,3)]
def main():
 evidence=json.loads((HERE/"DATASHEET_EVIDENCE.json").read_text());text=MOD.read_text()
 checks={
  "pdf_sha256":hashlib.sha256(PDF.read_bytes()).hexdigest()==evidence["source_sha256"],
  "status_non_frozen":"selection_frozen=false; production_ready=false" in evidence["status"],
  "axis_semantics_exact":evidence["evidence"]["body_nominal_xyz_mm"]==[4.6,2.3,1.8] and evidence["evidence"]["overall_Y_H035_mm"]==3.5 and evidence["evidence"]["local_axes"]["Z"]=="height above PCB",
  "H_not_Z":not any(k in evidence["evidence"] for k in ("overall_height_H035_mm","overall_height_tolerance_mm")),
  "internal_datums_not_projection":"not travel, Z height" in evidence["evidence"]["side_view_datum_interpretation"],
  "step_axis_spans_XYZ":bbox_span(HERE/"3dmodels/GT-TC020A-H035-L1-envelope.step")==[4.6,3.5,1.8],
  "pin1_exact":'(pad "1" smd rect (at -1.7 -1.33) (size 0.6 0.94)' in text,
  "pin2_exact":'(pad "2" smd rect (at 1.7 -1.33) (size 0.6 0.94)' in text,
  "mechanical_land_exact":'(pad "" smd rect (at 0 -1.33) (size 1.54 0.94)' in text,
  "two_0p77_holes":len(re.findall(r'np_thru_hole circle \(at (?:-2\.175|2\.175) 0\) \(size 0\.77 0\.77\) \(drill 0\.77\)',text))==2,
  "two_0p70_holes":len(re.findall(r'np_thru_hole circle \(at (?:-0\.85|0\.85) 0\) \(size 0\.70 0\.70\) \(drill 0\.70\)',text))==2,
  "press_direction_marked":'PRESS' in text,
  "fab_body_axes_exact":'(fp_rect (start -2.3 -1.15) (end 2.3 1.15)' in text,
  "courtyard_present":'F.CrtYd' in text,"fab_present":'F.Fab' in text,"silk_present":'F.SilkS' in text,
 }
 with tempfile.TemporaryDirectory() as td:
  cp=subprocess.run([str(KICAD),"fp","export","svg","--layers","F.Cu,F.Paste,F.Mask,F.SilkS,F.Fab,F.CrtYd","--output",td,str(MOD.parent)],capture_output=True,text=True)
  checks["kicad10_parse_and_svg_export"]=cp.returncode==0 and any(Path(td).glob("*.svg"))
 result={"status":"PASS" if all(checks.values()) else "FAIL","selection_frozen":False,"production_ready":False,"checks":checks,"pdf_page_evidence":[1],"pad_count_electrical":2,"mechanical_solder_land_count":1,"npth_count":4}
 (HERE/"verification.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n");print(json.dumps(result,ensure_ascii=False));return 0 if result["status"]=="PASS" else 1
if __name__=="__main__":raise SystemExit(main())
