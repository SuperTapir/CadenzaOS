#!/usr/bin/env python3
"""Generate non-frozen Power/Lock edge-button mechanical candidates.

The geometry is a fit-check around the shared 130 x 60 mm PCB datum and the
existing L1 front shell.  It deliberately models a generic edge-facing,
momentary switch envelope; it is not a production switch or footprint.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
L1_FRONT = REPO / "hardware/cadenza/mechanical/l1-fit-check/generated/l1-front-fitcheck.step"

PCB = (0.0, 0.0, 130.0, 60.0)
MOUNT_HOLES = ((5.0, 5.0), (5.0, 55.0), (125.0, 5.0), (125.0, 55.0))
MOUNT_KEEPOUT_R = 5.0
SCREEN_PANEL = (33.6, 8.59, 96.4, 51.41)
FPC_RELIEF = (60.18, 50.8, 69.82, 58.0)

# These are fit-check assumptions, not a selected switch's dimensions.
PROXY = {
    "cap_diameter_mm": 5.6,
    "cap_axial_length_mm": 5.2,
    "cap_to_guide_radial_clearance_mm": 0.40,
    "guide_wall_mm": 1.00,
    "guard_side_clearance_mm": 0.75,
    "guard_outer_recess_mm": 0.80,
    "assumed_travel_mm": 1.00,
    "switch_body_xyz_mm": [6.6, 6.6, 5.4],
    "switch_body_to_wall_clearance_mm": 0.60,
    "sidewall_proxy_z_mm": [-3.0, 6.8],
    "button_axis_z_mm": 2.7,
    "pcb_component_side_z_mm": 0.4,
}

VARIANTS = {
    "a-top-edge": {
        "intent": "顶边右侧，食指容易找到；避开 Sharp FPC 与右上安装孔",
        "axis": "+Y",
        "centre_xy_mm": [105.0, 61.0],
        "body_centre_xy_mm": [105.0, 55.6],
        "rest_axis_range_mm": [59.0, 64.2],
        "guard_outer_axis_mm": 65.0,
    },
    "b-right-edge": {
        "intent": "右侧偏下，拇指/食指可触达；与右侧编码器候选错开",
        "axis": "+X",
        "centre_xy_mm": [129.0, 18.0],
        "body_centre_xy_mm": [124.1, 18.0],
        "rest_axis_range_mm": [127.4, 132.6],
        "guard_outer_axis_mm": 133.4,
    },
}


def load_api():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRep import BRep_Builder
    from OCP.BRepAlgoAPI import BRepAlgoAPI_Common, BRepAlgoAPI_Cut, BRepAlgoAPI_Fuse
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.BRepPrimAPI import BRepPrimAPI_MakeBox, BRepPrimAPI_MakeCylinder
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_AsIs, STEPControl_Reader, STEPControl_Writer
    from OCP.StlAPI import StlAPI_Writer
    from OCP.TopoDS import TopoDS_Compound
    from OCP.gp import gp_Ax2, gp_Dir, gp_Pnt
    return locals()


def read_step(api, path: Path):
    reader = api["STEPControl_Reader"]()
    if reader.ReadFile(str(path)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"Cannot read {path}")
    reader.TransferRoots()
    shape = reader.OneShape()
    if shape.IsNull():
        raise RuntimeError(f"Null STEP: {path}")
    return shape


def box(api, x1, y1, z1, x2, y2, z2):
    return api["BRepPrimAPI_MakeBox"](api["gp_Pnt"](x1, y1, z1), x2-x1, y2-y1, z2-z1).Shape()


def cyl(api, origin, direction, radius, length):
    axis = api["gp_Ax2"](api["gp_Pnt"](*origin), api["gp_Dir"](*direction))
    return api["BRepPrimAPI_MakeCylinder"](axis, radius, length).Shape()


def cut(api, base, tool):
    op = api["BRepAlgoAPI_Cut"](base, tool); op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean cut failed")
    return op.Shape()


def fuse(api, a, b):
    op = api["BRepAlgoAPI_Fuse"](a, b); op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean fuse failed")
    return op.Shape()


def compound(api, shapes):
    result = api["TopoDS_Compound"](); builder = api["BRep_Builder"](); builder.MakeCompound(result)
    for shape in shapes:
        builder.Add(result, shape)
    return result


def common_volume(api, a, b):
    op = api["BRepAlgoAPI_Common"](a, b); op.Build()
    if not op.IsDone():
        raise RuntimeError("Boolean common failed")
    props = api["GProp_GProps"](); api["BRepGProp"].VolumeProperties_s(op.Shape(), props)
    return props.Mass()


def validate(api, label, shape):
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"Invalid shape: {label}")


def write_pair(api, shape, stem: Path):
    validate(api, stem.name, shape)
    step = api["STEPControl_Writer"](); step.Transfer(shape, api["STEPControl_AsIs"])
    if step.Write(str(stem.with_suffix(".step"))) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"STEP write failed: {stem}")
    api["BRepMesh_IncrementalMesh"](shape, 0.08, False, 0.35, True)
    if not api["StlAPI_Writer"]().Write(shape, str(stem.with_suffix(".stl"))):
        raise RuntimeError(f"STL write failed: {stem}")


def geometry(api, name, v, base):
    r = PROXY["cap_diameter_mm"] / 2
    inner_r = r + PROXY["cap_to_guide_radial_clearance_mm"]
    outer_r = inner_r + PROXY["guide_wall_mm"]
    z = PROXY["button_axis_z_mm"]
    travel = PROXY["assumed_travel_mm"]
    bx, by, bz = PROXY["switch_body_xyz_mm"]

    if v["axis"] == "+Y":
        x = v["centre_xy_mm"][0]; a1, a2 = v["rest_axis_range_mm"]
        cap = cyl(api, (x, a1, z), (0, 1, 0), r, a2-a1)
        pressed = cyl(api, (x, a1-travel, z), (0, 1, 0), r, a2-a1)
        sweep = cyl(api, (x, a1-travel, z), (0, 1, 0), r, a2-a1+travel)
        opening = cyl(api, (x, 58.0, z), (0, 1, 0), inner_r, 6.8)
        outer = cyl(api, (x, 58.8, z), (0, 1, 0), outer_r, 4.2)
        inner = cyl(api, (x, 58.7, z), (0, 1, 0), inner_r, 4.4)
        guide = cut(api, outer, inner)
        wall = box(api, x-7.5, 59.5, -3.0, x+7.5, 62.8, 6.8)
        tangent = r + PROXY["guard_side_clearance_mm"]
        guard1 = box(api, x-tangent-2.0, 60.2, -0.8, x-tangent, v["guard_outer_axis_mm"], 6.2)
        guard2 = box(api, x+tangent, 60.2, -0.8, x+tangent+2.0, v["guard_outer_axis_mm"], 6.2)
        cx, cy = v["body_centre_xy_mm"]
        body = box(api, cx-bx/2, cy-by/2, PROXY["pcb_component_side_z_mm"], cx+bx/2, cy+by/2, PROXY["pcb_component_side_z_mm"]+bz)
    else:
        y = v["centre_xy_mm"][1]; a1, a2 = v["rest_axis_range_mm"]
        cap = cyl(api, (a1, y, z), (1, 0, 0), r, a2-a1)
        pressed = cyl(api, (a1-travel, y, z), (1, 0, 0), r, a2-a1)
        sweep = cyl(api, (a1-travel, y, z), (1, 0, 0), r, a2-a1+travel)
        # Extend the aperture inward past the assumed fully-pressed cap.
        opening = cyl(api, (125.8, y, z), (1, 0, 0), inner_r, 8.0)
        outer = cyl(api, (127.8, y, z), (1, 0, 0), outer_r, 4.2)
        inner = cyl(api, (127.7, y, z), (1, 0, 0), inner_r, 4.4)
        guide = cut(api, outer, inner)
        wall = box(api, 128.0, y-7.5, -3.0, 131.0, y+7.5, 6.8)
        tangent = r + PROXY["guard_side_clearance_mm"]
        guard1 = box(api, 128.7, y-tangent-2.0, -0.8, v["guard_outer_axis_mm"], y-tangent, 6.2)
        guard2 = box(api, 128.7, y+tangent, -0.8, v["guard_outer_axis_mm"], y+tangent+2.0, 6.2)
        cx, cy = v["body_centre_xy_mm"]
        body = box(api, cx-bx/2, cy-by/2, PROXY["pcb_component_side_z_mm"], cx+bx/2, cy+by/2, PROXY["pcb_component_side_z_mm"]+bz)

    shell_with_wall = fuse(api, base, wall)
    shell_cut = cut(api, shell_with_wall, opening)
    guard = compound(api, [guard1, guard2])
    interface = compound(api, [shell_cut, guide, guard])
    switch_keepout = compound(api, [body, sweep])
    assembly = compound(api, [interface, cap, body])
    return {"enclosure-interface": interface, "button-cap-proxy": cap, "guide-proxy": guide,
            "guarded-recess-proxy": guard, "switch-keepout-proxy": switch_keepout,
            "actuation-sweep-proxy": sweep, "assembly": assembly}, pressed


def clearance_xy(rect, body_rect):
    ax1, ay1, ax2, ay2 = rect; bx1, by1, bx2, by2 = body_rect
    dx = max(ax1-bx2, bx1-ax2, 0.0); dy = max(ay1-by2, by1-ay2, 0.0)
    return math.hypot(dx, dy) if dx or dy else -min(ax2-bx1, bx2-ax1, ay2-by1, by2-ay1)


def preview_png(path: Path, name, v):
    from PIL import Image, ImageDraw
    scale = 7; margin = 35
    im = Image.new("RGB", (130*scale+2*margin, 60*scale+150), "#f5f4ef"); d = ImageDraw.Draw(im)
    def p(x, y): return (margin+x*scale, margin+(60-y)*scale)
    d.rectangle([p(0,60), p(130,0)], outline="#202020", width=3)
    d.rectangle([p(SCREEN_PANEL[0],SCREEN_PANEL[3]), p(SCREEN_PANEL[2],SCREEN_PANEL[1])], fill="#dce8d6", outline="#375033", width=2)
    for x,y in MOUNT_HOLES:
        q=p(x,y); rr=MOUNT_KEEPOUT_R*scale; d.ellipse([q[0]-rr,q[1]-rr,q[0]+rr,q[1]+rr], outline="#888888", width=2)
    x,y=v["centre_xy_mm"]; q=p(x,y); rr=PROXY["cap_diameter_mm"]*scale/2
    d.ellipse([q[0]-rr,q[1]-rr,q[0]+rr,q[1]+rr], fill="#d96742", outline="#7c2913", width=2)
    cx,cy=v["body_centre_xy_mm"]; bx,by,_=PROXY["switch_body_xyz_mm"]
    d.rectangle([p(cx-bx/2,cy+by/2),p(cx+bx/2,cy-by/2)], fill="#80a9ce", outline="#244b70", width=2)
    d.text((margin, 8), f"{name} - candidate only / NOT FROZEN", fill="#111111")
    d.text((margin, 60*scale+58), "orange: cap proxy   blue: PCB switch-body keepout   green: Sharp panel", fill="#222222")
    d.text((margin, 60*scale+82), f"assumed travel {PROXY['assumed_travel_mm']:.1f} mm; real switch + PCB/case gap: TO VERIFY", fill="#8a1f11")
    im.save(path)


def render_stl_iso(stl_path: Path, png_path: Path, name: str):
    """Render the generated assembly STL itself, not an illustrative redraw."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    vertices=[]
    for line in stl_path.read_text(encoding="ascii", errors="strict").splitlines():
        fields=line.strip().split()
        if len(fields)==4 and fields[0]=="vertex": vertices.append(tuple(float(x) for x in fields[1:]))
    if not vertices or len(vertices)%3: raise RuntimeError(f"Cannot parse ASCII STL: {stl_path}")
    tri=np.asarray(vertices,dtype=float).reshape((-1,3,3))
    if len(tri)>14000: tri=tri[::math.ceil(len(tri)/14000)]
    xyz=tri.reshape((-1,3)); lo=xyz.min(axis=0); hi=xyz.max(axis=0); span=hi-lo
    fig=plt.figure(figsize=(10,5.2),dpi=150); ax=fig.add_subplot(111,projection="3d")
    mesh=Poly3DCollection(tri,facecolor="#9aafbd",edgecolor="#40525e",linewidth=0.05,alpha=0.92)
    ax.add_collection3d(mesh)
    centre=(lo+hi)/2; radius=max(span)/2*1.05
    ax.set_xlim(centre[0]-radius,centre[0]+radius); ax.set_ylim(centre[1]-radius,centre[1]+radius); ax.set_zlim(centre[2]-radius,centre[2]+radius)
    ax.set_box_aspect((1,1,0.28)); ax.view_init(elev=30,azim=-58)
    ax.set_title(f"{name} generated assembly STL - NOT FROZEN",fontsize=11)
    ax.set_axis_off(); fig.tight_layout(); fig.savefig(png_path,bbox_inches="tight",facecolor="#f5f4ef"); plt.close(fig)


def main():
    api = load_api(); base = read_step(api, L1_FRONT); out = HERE / "generated"; out.mkdir(parents=True, exist_ok=True)
    screen_keepout = box(api, SCREEN_PANEL[0], SCREEN_PANEL[1], -5, SCREEN_PANEL[2], SCREEN_PANEL[3], 10)
    fpc_keepout = box(api, FPC_RELIEF[0], FPC_RELIEF[1], -5, FPC_RELIEF[2], FPC_RELIEF[3], 10)
    mounts = compound(api, [cyl(api,(x,y,-5),(0,0,1),MOUNT_KEEPOUT_R,15) for x,y in MOUNT_HOLES])
    # Conservative union of all current L2 input body/cap positions.
    l2 = compound(api, [
        cyl(api,(19,29,-5),(0,0,1),5.25,15), cyl(api,(18,43,-5),(0,0,1),4.0,15), cyl(api,(112,30,-5),(0,0,1),9.0,15),
        cyl(api,(20,27,-5),(0,0,1),5.25,15), cyl(api,(20,39,-5),(0,0,1),4.0,15), cyl(api,(111,30,-5),(0,0,1),8.0,15),
        cyl(api,(18,31,-5),(0,0,1),5.25,15), cyl(api,(23,44,-5),(0,0,1),4.0,15), cyl(api,(112,32,-5),(0,0,1),9.0,15),
    ])
    manifest = {
        "status": "candidate only; selection_frozen=false; production_ready=false",
        "coordinate_system": "shared PCB XY: x=0..130 left-to-right, y=0..60 bottom-to-top; L1 front z≈-3..0",
        "inherited_datums": {"pcb_xy_mm": PCB, "mount_holes_xy_mm": MOUNT_HOLES, "screen_panel_xy_mm": SCREEN_PANEL, "fpc_relief_xy_mm": FPC_RELIEF},
        "proxy_dimensions_not_component_selection": PROXY,
        "variants": {},
        "verification_limits": [
            "真实开关型号、封装和 JLCPCB/LCSC 可采购性未确认",
            "真实执行头形状、总行程、操作力和寿命未确认",
            "PCB 元件面到完整外壳侧壁/顶边的装配距离未实测",
            "这里只继承 L1 前壳；后壳、分模线、螺柱和装配顺序尚未冻结",
            "按帽配合、打印材料收缩和导向间隙必须用实物样件验证",
            "Power/Lock 电气长按关机行为不由本机械候选证明",
        ],
    }
    report_rows=[]
    for name,v in VARIANTS.items():
        folder=out/name; folder.mkdir(parents=True,exist_ok=True)
        shapes,pressed=geometry(api,name,v,base)
        for part,shape in shapes.items(): write_pair(api,shape,folder/f"{name}-{part}")
        preview_png(folder/f"{name}-assembly-preview.png",name,v)
        render_stl_iso(folder/f"{name}-assembly.stl",folder/f"{name}-assembly-iso.png",name)
        switch=shapes["switch-keepout-proxy"]
        collisions={
            "screen_vs_switch_keepout_mm3": common_volume(api,screen_keepout,switch),
            "fpc_relief_vs_switch_keepout_mm3": common_volume(api,fpc_keepout,switch),
            "mount_keepouts_vs_switch_keepout_mm3": common_volume(api,mounts,switch),
            "l2_controls_vs_switch_keepout_mm3": common_volume(api,l2,switch),
            "enclosure_interface_vs_rest_cap_mm3": common_volume(api,shapes["enclosure-interface"],shapes["button-cap-proxy"]),
            "enclosure_interface_vs_pressed_cap_mm3": common_volume(api,shapes["enclosure-interface"],pressed),
            "guard_vs_rest_cap_mm3": common_volume(api,shapes["guarded-recess-proxy"],shapes["button-cap-proxy"]),
        }
        if any(abs(x)>1e-5 for x in collisions.values()): raise RuntimeError(f"{name} collision: {collisions}")
        bx,by,_=PROXY["switch_body_xyz_mm"]; cx,cy=v["body_centre_xy_mm"]; bodyrect=(cx-bx/2,cy-by/2,cx+bx/2,cy+by/2)
        metrics={"screen_panel_xy_clearance_mm":round(clearance_xy(SCREEN_PANEL,bodyrect),3),"fpc_relief_xy_clearance_mm":round(clearance_xy(FPC_RELIEF,bodyrect),3),"guard_protrudes_beyond_cap_mm":PROXY["guard_outer_recess_mm"]}
        rec={**v,"metrics":metrics,"hard_collision_common_volume_mm3":collisions}; manifest["variants"][name]=rec
        (folder/f"{name}-parameters.json").write_text(json.dumps(rec,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        report_rows.append(f"| {name} | {v['axis']} | `{tuple(v['centre_xy_mm'])}` | {metrics['screen_panel_xy_clearance_mm']:.1f} mm | {metrics['fpc_relief_xy_clearance_mm']:.1f} mm | 0 mm³ |")
    (out/"power-lock-candidates.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    report = """# Power/Lock 候选碰撞报告\n\n> 状态：非冻结候选；不能用于生产。\n\n| 候选 | 出力方向 | 按帽中心 XY | 屏幕 XY 间隙 | FPC XY 间隙 | 所测硬碰撞最大值 |\n|---|---|---:|---:|---:|---:|\n""" + "\n".join(report_rows) + """\n\n碰撞集合包含：Sharp 面板、FPC 避让、四个安装孔 5 mm 半径 keepout、三套 L2 输入代理、静止/按下按帽与外壳接口、按帽与防误触护栏。全部公共体积为 `0 mm³`。这只证明当前代理互不穿透，不证明真实开关可以安装、焊接或达到所需手感。\n"""
    (HERE/"COLLISION_REPORT.md").write_text(report,encoding="utf-8")
    print(json.dumps({"status":"pass","variants":list(VARIANTS),"selection_frozen":False,"production_ready":False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
