#!/usr/bin/env python3
"""Audit L1 planar placement options against the protected reference PCB.

This is deliberately a placement-only candidate.  It does not edit a board,
assign an FPC footprint, or claim Z-stack/mechanical production readiness.
Run with KiCad's bundled Python so pcbnew uses the same parser as KiCad 10.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

import pcbnew


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
BOARD = REPO / "hardware/cadenza/working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb"
FPC_VARIANTS = REPO / "hardware/cadenza/mechanical/l1-fit-check/variants/generated/fpc-direction-variants.json"
POWER_FIT = REPO / "hardware/cadenza/mechanical/power-lock-s1-fit/generated/power-lock-s1-fit.json"
OUTPUT_JSON = HERE / "placement-feasibility.json"
OUTPUT_SVG = HERE / "placement-feasibility.svg"

DELETED_REFS = {"FPC1", "U6", "C20", "C21", "Q2", "R13", "R14", "SW1", "KEY1"}
USB_SHIFT_X_MM = 16.2


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def round_rect(rect: list[float]) -> list[float]:
    return [round(value, 3) for value in rect]


def overlap(a: list[float], b: list[float]) -> dict[str, float | bool]:
    width = max(0.0, min(a[2], b[2]) - max(a[0], b[0]))
    height = max(0.0, min(a[3], b[3]) - max(a[1], b[1]))
    return {
        "overlaps": width > 1e-9 and height > 1e-9,
        "width_mm": round(width, 3),
        "height_mm": round(height, 3),
        "area_mm2": round(width * height, 3),
    }


def main() -> int:
    board = pcbnew.LoadBoard(str(BOARD))
    edge = board.GetBoardEdgesBoundingBox()
    xmin = pcbnew.ToMM(edge.GetX())
    ymin = pcbnew.ToMM(edge.GetY())
    xmax = pcbnew.ToMM(edge.GetRight())
    ymax = pcbnew.ToMM(edge.GetBottom())
    width = xmax - xmin
    height = ymax - ymin

    footprints: dict[str, dict[str, object]] = {}
    for footprint in board.GetFootprints():
        ref = footprint.GetReference()
        bbox = footprint.GetBoundingBox(False, False)
        x1 = pcbnew.ToMM(bbox.GetX()) - xmin
        x2 = pcbnew.ToMM(bbox.GetRight()) - xmin
        y1 = ymax - pcbnew.ToMM(bbox.GetBottom())
        y2 = ymax - pcbnew.ToMM(bbox.GetY())
        position = footprint.GetPosition()
        footprints[ref] = {
            "layer": board.GetLayerName(footprint.GetLayer()),
            "position_xy_mm": round_rect([
                pcbnew.ToMM(position.x) - xmin,
                ymax - pcbnew.ToMM(position.y),
            ]),
            "bbox_xy_mm": round_rect([x1, y1, x2, y2]),
        }

    # U4 is rotation 0 in the inherited board.  These antenna limits are the
    # explicit antenna-side silk region in the imported ESP32-S3-WROOM-1
    # footprint: local X ±9, local Y -16.395 .. -10.3555 mm.
    u4_x, u4_y = footprints["U4"]["position_xy_mm"]
    antenna = round_rect([u4_x - 9.0, u4_y + 10.3555, u4_x + 9.0, u4_y + 16.395])

    fpc_data = json.loads(FPC_VARIANTS.read_text(encoding="utf-8"))
    plus = fpc_data["variants"]["fpc-plus-y-bottom-contact"]["bend_keepout_bbox"]
    minus = fpc_data["variants"]["fpc-minus-y-bottom-contact"]["bend_keepout_bbox"]
    fpc_plus = round_rect([plus["xmin"], plus["ymin"], plus["xmax"], plus["ymax"]])
    fpc_minus = round_rect([minus["xmin"], minus["ymin"], minus["xmax"], minus["ymax"]])

    usb_original = list(footprints["USB1"]["bbox_xy_mm"])
    usb_shifted = round_rect([
        usb_original[0] + USB_SHIFT_X_MM,
        usb_original[1],
        usb_original[2] + USB_SHIFT_X_MM,
        usb_original[3],
    ])
    cc_target_x_mm = 91.0
    cc_shift_x_mm = cc_target_x_mm - footprints["R1"]["position_xy_mm"][0]
    r1_shifted = round_rect([
        footprints["R1"]["bbox_xy_mm"][0] + cc_shift_x_mm,
        footprints["R1"]["bbox_xy_mm"][1],
        footprints["R1"]["bbox_xy_mm"][2] + cc_shift_x_mm,
        footprints["R1"]["bbox_xy_mm"][3],
    ])
    r2_shifted = round_rect([
        footprints["R2"]["bbox_xy_mm"][0] + cc_shift_x_mm,
        footprints["R2"]["bbox_xy_mm"][1],
        footprints["R2"]["bbox_xy_mm"][2] + cc_shift_x_mm,
        footprints["R2"]["bbox_xy_mm"][3],
    ])

    power_data = json.loads(POWER_FIT.read_text(encoding="utf-8"))
    ax, ay = power_data["variants"]["top-edge-A"]["origin_xy_mm"]
    bx, by = power_data["variants"]["right-edge-B"]["origin_xy_mm"]
    # Full nominal body + unpressed actuator envelope.  Projection beyond the
    # PCB outline is intentional for a side-actuated enclosure button.
    power_a = round_rect([ax - 2.3, ay - 1.15, ax + 2.3, ay + 2.35])
    power_b = round_rect([bx - 1.15, by - 2.3, bx + 2.35, by + 2.3])

    retained = {
        ref: value for ref, value in footprints.items() if ref not in DELETED_REFS
    }

    def collisions(rect: list[float], ignored: set[str] | None = None) -> list[dict[str, object]]:
        ignored = ignored or set()
        found = []
        for ref, value in sorted(retained.items()):
            if ref in ignored:
                continue
            hit = overlap(rect, value["bbox_xy_mm"])
            if hit["overlaps"]:
                found.append({"reference": ref, "layer": value["layer"], **hit})
        return found

    findings = {
        "fpc_plus_y_vs_esp32_antenna": overlap(fpc_plus, antenna),
        "fpc_minus_y_vs_original_usb": overlap(fpc_minus, usb_original),
        "fpc_minus_y_vs_shifted_usb": overlap(fpc_minus, usb_shifted),
        "shifted_usb_vs_u3": overlap(usb_shifted, footprints["U3"]["bbox_xy_mm"]),
        "shifted_usb_planar_collisions": collisions(usb_shifted, {"USB1"}),
        "shifted_r1_planar_collisions": collisions(r1_shifted, {"R1", "USB1"}),
        "shifted_r2_planar_collisions": collisions(r2_shifted, {"R2", "USB1"}),
        "power_top_edge_A_planar_collisions": collisions(power_a),
        "power_right_edge_B_planar_collisions": collisions(power_b),
        "clearances_mm": {
            "fpc_minus_y_to_shifted_usb_x": round(usb_shifted[0] - fpc_minus[2], 3),
            "led1_to_shifted_usb_x": round(usb_shifted[0] - footprints["LED1"]["bbox_xy_mm"][2], 3),
            "shifted_usb_to_cc_resistors_x": round(r1_shifted[0] - usb_shifted[2], 3),
            "shifted_cc_resistors_to_u3_y": round(footprints["U3"]["bbox_xy_mm"][1] - r1_shifted[3], 3),
        },
    }

    checks = {
        "reference_board_exactly_77_footprints": len(footprints) == 77,
        "deleted_reference_set_exactly_9": len(DELETED_REFS) == 9 and DELETED_REFS <= set(footprints),
        "plus_y_fpc_conflicts_with_antenna": findings["fpc_plus_y_vs_esp32_antenna"]["overlaps"],
        "minus_y_fpc_conflicts_with_original_usb": findings["fpc_minus_y_vs_original_usb"]["overlaps"],
        "minus_y_fpc_clears_shifted_usb": not findings["fpc_minus_y_vs_shifted_usb"]["overlaps"],
        "shifted_usb_clears_u3": not findings["shifted_usb_vs_u3"]["overlaps"],
        "shifted_usb_has_no_other_planar_footprint_collision": not findings["shifted_usb_planar_collisions"],
        "shifted_usb_has_at_least_1mm_led1_clearance": findings["clearances_mm"]["led1_to_shifted_usb_x"] >= 1.0,
        "shifted_usb_has_at_least_5mm_fpc_planar_gap": findings["clearances_mm"]["fpc_minus_y_to_shifted_usb_x"] >= 5.0,
        "shifted_r1_has_no_other_planar_footprint_collision": not findings["shifted_r1_planar_collisions"],
        "shifted_r2_has_no_other_planar_footprint_collision": not findings["shifted_r2_planar_collisions"],
        "cc_resistors_have_at_least_1mm_usb_gap": findings["clearances_mm"]["shifted_usb_to_cc_resistors_x"] >= 1.0,
        "cc_resistors_have_at_least_3mm_u3_gap": findings["clearances_mm"]["shifted_cc_resistors_to_u3_y"] >= 3.0,
        "power_A_conflict_is_exactly_retained_KEY2": [
            item["reference"] for item in findings["power_top_edge_A_planar_collisions"]
        ] == ["KEY2"],
        "power_B_has_no_retained_planar_footprint_collision": not findings["power_right_edge_B_planar_collisions"],
    }

    result = {
        "schema_version": 1,
        "status": "PASS_PLACEMENT_CANDIDATE" if all(checks.values()) else "FAIL",
        "selection_frozen": False,
        "production_ready": False,
        "pcb_created": False,
        "sources": {
            "reference_board": str(BOARD.relative_to(REPO)),
            "reference_board_sha256": sha256(BOARD),
            "fpc_variants": str(FPC_VARIANTS.relative_to(REPO)),
            "fpc_variants_sha256": sha256(FPC_VARIANTS),
            "power_fit": str(POWER_FIT.relative_to(REPO)),
            "power_fit_sha256": sha256(POWER_FIT),
        },
        "coordinate_system": {
            "origin": "inherited PCB lower-left rectangular extent",
            "x": "right",
            "y": "toward inherited PCB upper edge",
            "board_extent_xy_mm": round_rect([0.0, 0.0, width, height]),
        },
        "preserved": {
            "board_outline": True,
            "four_mounting_holes": True,
            "esp32_module_position": footprints["U4"]["position_xy_mm"],
            "esp32_antenna_planar_keepout_xy_mm": antenna,
            "reference_footprints_fixed_except": ["USB1", "R1", "R2"],
        },
        "candidate": {
            "screen_rotation_in_plane_deg": 180,
            "fpc_exit": "-Y / lower edge",
            "fpc_connector": "FH34SRJ dual-contact provisional; Pin-1 rotation still requires physical continuity",
            "fpc_bend_keepout_xy_mm": fpc_minus,
            "usb1_move": {
                "topology": "unchanged",
                "delta_xy_mm": [USB_SHIFT_X_MM, 0.0],
                "original_bbox_xy_mm": usb_original,
                "candidate_bbox_xy_mm": usb_shifted,
                "new_position_xy_mm": [
                    round(footprints["USB1"]["position_xy_mm"][0] + USB_SHIFT_X_MM, 3),
                    footprints["USB1"]["position_xy_mm"][1],
                ],
            },
            "usb_cc_resistor_moves": {
                "references": ["R1", "R2"],
                "target_x_mm": cc_target_x_mm,
                "r1_bbox_xy_mm": r1_shifted,
                "r2_bbox_xy_mm": r2_shifted,
                "topology": "unchanged 5.1 kOhm CC1/CC2 pulldowns",
            },
            "power_lock_preference": "right-edge-B; top-edge-A excluded by retained KEY2 planar collision",
            "power_lock_A_envelope_xy_mm": power_a,
            "power_lock_B_envelope_xy_mm": power_b,
            "j_disp1_pin1_variants": ["rotation_0_candidate", "rotation_180_candidate"],
        },
        "findings": findings,
        "checks": checks,
        "limits": [
            "Planar rectangles do not prove Z clearance or assembly access.",
            "USB1 relocation requires rerouting D+/D-, VBUS, CC1/CC2 and shield/ground while preserving the reference topology.",
            "FH34SRJ dual-contact removes contact-face dependence but not Pin 1 direction dependence.",
            "The FPC connector layer, latch opening space, exact 3D model and folded cable path remain unfrozen.",
            "Power/Lock A and B extend intentionally beyond the nominal PCB extent; enclosure wall/cap/guide fit still requires a print.",
        ],
    }
    OUTPUT_JSON.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def svg_rect(rect: list[float], css: str, label: str) -> str:
        x1, y1, x2, y2 = rect
        # SVG Y is downward; mirror the mechanical Y coordinate.
        sy = height - y2
        return (
            f'<rect x="{x1:.3f}" y="{sy:.3f}" width="{x2-x1:.3f}" height="{y2-y1:.3f}" class="{css}"/>'
            f'<text x="{x1+0.7:.3f}" y="{sy+2.2:.3f}" class="label">{label}</text>'
        )

    screen = [33.6, 8.59, 96.4, 51.41]
    svg = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="-4 -4 {width+8:.3f} {height+8:.3f}">
<style>
.board{{fill:#294436;stroke:#111;stroke-width:.45}} .screen{{fill:#bed9e8;fill-opacity:.42;stroke:#277da1;stroke-width:.35}}
.antenna{{fill:#e63946;fill-opacity:.55;stroke:#9d0208;stroke-width:.3}} .old{{fill:#ffb703;fill-opacity:.45;stroke:#9c6500;stroke-width:.3}}
.candidate{{fill:#52b788;fill-opacity:.55;stroke:#1b4332;stroke-width:.3}} .fpc{{fill:#f72585;fill-opacity:.45;stroke:#9d174d;stroke-width:.3}}
.power{{fill:#9b5de5;fill-opacity:.45;stroke:#5a189a;stroke-width:.3}} .hole{{fill:#fff;stroke:#111;stroke-width:.25}}
.label{{font-family:sans-serif;font-size:1.55px;fill:#111}} .title{{font-family:sans-serif;font-size:2.1px;font-weight:bold;fill:#111}}
</style>
<rect x="0" y="0" width="{width:.3f}" height="{height:.3f}" rx="5" class="board"/>
{svg_rect(screen, 'screen', 'Sharp panel')}
{svg_rect(antenna, 'antenna', 'ESP32 antenna')}
{svg_rect(fpc_plus, 'old', 'FPC +Y conflict')}
{svg_rect(fpc_minus, 'fpc', 'FPC -Y candidate')}
{svg_rect(usb_original, 'old', 'USB original')}
{svg_rect(footprints['LED1']['bbox_xy_mm'], 'old', 'LED1')}
{svg_rect(footprints['KEY2']['bbox_xy_mm'], 'old', 'KEY2')}
{svg_rect(usb_shifted, 'candidate', f'USB +{USB_SHIFT_X_MM:g} mm')}
{svg_rect(r1_shifted, 'candidate', 'R1 CC1')}
{svg_rect(r2_shifted, 'candidate', 'R2 CC2')}
{svg_rect(power_a, 'power', 'Power A')}
{svg_rect(power_b, 'power', 'Power B')}
<circle cx="5" cy="{height-5:.3f}" r="1.1" class="hole"/><circle cx="5" cy="{height-55:.3f}" r="1.1" class="hole"/>
<circle cx="125" cy="{height-5:.3f}" r="1.1" class="hole"/><circle cx="125" cy="{height-55:.3f}" r="1.1" class="hole"/>
<text x="2" y="-1" class="title">Cadenza L1 placement candidate — not frozen</text>
</svg>'''
    OUTPUT_SVG.write_text(svg, encoding="utf-8")
    print(json.dumps({"status": result["status"], "checks": len(checks), "output": str(OUTPUT_JSON)}))
    return 0 if result["status"] == "PASS_PLACEMENT_CANDIDATE" else 1


if __name__ == "__main__":
    raise SystemExit(main())
