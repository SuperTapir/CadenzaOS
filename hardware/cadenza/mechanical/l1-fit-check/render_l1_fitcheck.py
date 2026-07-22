#!/usr/bin/env python3
"""Render readable L1 fit-check plan and provisional centre-section diagrams."""

from __future__ import annotations

import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, Rectangle


HERE = Path(__file__).resolve().parent
PARAMS = json.loads((HERE/"generated/l1-front-fitcheck-parameters.json").read_text())
PCB = json.loads((HERE/"generated/candidate-pcb-mechanical.json").read_text())
OUT = HERE/"render"


def rect(ax, xyxy, color, label, alpha=0.25, hatch=None, lw=1.5):
    x1,y1,x2,y2=xyxy
    patch=Rectangle((x1,y1),x2-x1,y2-y1,facecolor=color,edgecolor=color,
                    linewidth=lw,alpha=alpha,hatch=hatch,label=label)
    ax.add_patch(patch)
    return patch


def main() -> int:
    OUT.mkdir(parents=True,exist_ok=True)
    panel=PARAMS["sharp_panel_xy"]; window=PARAMS["new_window_xy"]
    fpc=PARAMS["fpc_flat_xy"]; bend=PARAMS["fpc_bend_keepout_xy"]
    slot=PARAMS["front_fpc_pass_through_slot_xy"]
    j=PARAMS["j_disp1"]["courtyard_xy_enclosure_mm"]
    usb=PARAMS["usb1"]["courtyard_xy_enclosure_mm"]

    fig,ax=plt.subplots(figsize=(13,6),dpi=180)
    rect(ax,(0,0,130,60),"#243447","PCB outline",0.05,lw=2)
    rect(ax,panel,"#2ca25f","Sharp glass",0.20)
    rect(ax,window,"#3182bd","view window",0.18)
    rect(ax,fpc,"#fdae6b","flat FPC datum",0.55)
    rect(ax,bend,"#e6550d","fold/bend keepout",0.28,"///")
    rect(ax,slot,"#31a354","front FPC slot candidate",0.12,"xx")
    rect(ax,j,"#756bb1","J_DISP1 courtyard",0.35)
    rect(ax,usb,"#de2d26","USB1 courtyard",0.35)
    for hole in PCB["mounting_holes"]:
        ax.add_patch(Circle(hole["centre_enclosure_mm"],hole["pcb_drill_diameter_mm"]/2,
                            fill=False,color="#111",linewidth=1.3))
    ax.annotate(f"courtyard gap {PARAMS['j_disp1_to_usb1_courtyard_gap_y_mm']:.3f} mm",
                xy=(66.5,(usb[3]+j[1])/2),xytext=(75,4.2),
                arrowprops={"arrowstyle":"->","color":"#111"},fontsize=9)
    ax.text(66.5,30,"Sharp shifted +1.5 X / +1.5 enclosure Y",ha="center",va="center",fontsize=10)
    ax.set(xlim=(-2,132),ylim=(-2,62),aspect="equal",xlabel="enclosure X (mm)",ylabel="enclosure Y (mm)",
           title="L1 screen-only PCB / enclosure plan — physical FPC route still pending")
    ax.legend(loc="upper center",bbox_to_anchor=(0.5,-0.12),ncol=4,frameon=False)
    ax.grid(True,alpha=0.15)
    fig.tight_layout()
    fig.savefig(OUT/"l1-screen-pcb-clearance-plan.png",bbox_inches="tight")
    plt.close(fig)

    z=PARAMS["z_stack_proxy"]; face=z["pcb_component_face_z_mm"]
    fig,ax=plt.subplots(figsize=(11,5),dpi=180)
    rect(ax,(0,-3.0003,slot[1],0.0004),"#7f8c8d","front-cover Z band",0.20)
    rect(ax,(window[3],-3.0003,60,0.0004),"#7f8c8d","_front-cover Z band",0.20)
    rect(ax,(panel[1],PARAMS["screen_front_z"],panel[3],PARAMS["screen_rear_z"]),
         "#2ca25f","Sharp glass",0.30)
    rect(ax,(fpc[1],PARAMS["screen_rear_z"],fpc[3],PARAMS["screen_rear_z"]+0.30),
         "#fdae6b","flat FPC",0.55)
    rect(ax,(bend[1],PARAMS["screen_rear_z"]-0.1,bend[3],face-1.0+0.9),
         "#e6550d","bend keepout",0.22,"///")
    rect(ax,(0,face,60,face+z["pcb_thickness_mm"]),"#243447","PCB proxy",0.16)
    rect(ax,(j[1],face-z["fh34_height_mm"],j[3],face),"#756bb1","FH34 envelope",0.35)
    rect(ax,(usb[1],face-z["usb1_height_mm"],usb[3],face),"#de2d26","USB height proxy",0.35)
    ax.axhline(face,color="#222",lw=0.8,ls="--")
    ax.text(30,face+0.15,"PCB F.Cu face Z is a visualisation proxy, not measured",ha="center",fontsize=9)
    ax.set(xlim=(-1,61),ylim=(-6,5),xlabel="enclosure Y (mm)",ylabel="Z (mm)",
           title="L1 centre-section proxy — do not release without measured PCB seating Z")
    ax.grid(True,alpha=0.15); ax.legend(loc="upper right",fontsize=8)
    fig.tight_layout()
    fig.savefig(OUT/"l1-screen-pcb-provisional-z-section.png",bbox_inches="tight")
    plt.close(fig)
    print(json.dumps({"status":"PASS","outputs":[
        str(OUT/"l1-screen-pcb-clearance-plan.png"),
        str(OUT/"l1-screen-pcb-provisional-z-section.png")]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
