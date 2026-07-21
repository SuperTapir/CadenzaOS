#!/usr/bin/env python3
"""Finish the small set of nets left by Freerouting using grid A* routing."""

from __future__ import annotations

import heapq
import math
from pathlib import Path

import pcbnew


ROOT = Path(__file__).parent / "production-v41-electrical-audit"
SOURCE = ROOT / "cadenza-d1-v41.kicad_pcb"
OUTPUT = ROOT / "cadenza-d1-v41-critical-pre-route.kicad_pcb"
GRID = 0.25
BOUNDS = (52.0, 51.0, 163.0, 117.0)


def mm(value):
    return pcbnew.ToMM(value)


def point(xy):
    return pcbnew.VECTOR2I(pcbnew.FromMM(xy[0]), pcbnew.FromMM(xy[1]))


def add_track(board, net, layer, a, b, width=0.2):
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(point(a))
    track.SetEnd(point(b))
    track.SetWidth(pcbnew.FromMM(width))
    track.SetLayer(layer)
    track.SetNet(net)
    board.Add(track)


def add_via(board, net, xy, diameter=0.6, drill=0.3):
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(point(xy))
    via.SetWidth(pcbnew.FromMM(diameter))
    via.SetDrill(pcbnew.FromMM(drill))
    via.SetViaType(pcbnew.VIATYPE_THROUGH)
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    board.Add(via)


def segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def nodes_in_box(left, top, right, bottom):
    x0, y0 = grid_node((left, top))
    x1, y1 = grid_node((right, bottom))
    for gx in range(max(0, x0 - 1), min(round((BOUNDS[2] - BOUNDS[0]) / GRID), x1 + 1) + 1):
        for gy in range(max(0, y0 - 1), min(round((BOUNDS[3] - BOUNDS[1]) / GRID), y1 + 1) + 1):
            yield (gx, gy)


def build_blocked(board, layer, net_code):
    result = set()
    for item in board.GetTracks():
        if item.GetNetCode() == net_code:
            continue
        if item.Type() == pcbnew.PCB_VIA_T:
            pos = item.GetPosition()
            cx, cy = mm(pos.x), mm(pos.y)
            radius = mm(item.GetWidth(layer)) / 2 + 0.23
            for node in nodes_in_box(cx - radius, cy - radius, cx + radius, cy + radius):
                x, y = grid_xy(node)
                if math.hypot(x - cx, y - cy) < radius:
                    result.add(node)
        elif item.GetLayer() == layer:
            a, b = item.GetStart(), item.GetEnd()
            ax, ay, bx, by = mm(a.x), mm(a.y), mm(b.x), mm(b.y)
            radius = mm(item.GetWidth()) / 2 + 0.23
            for node in nodes_in_box(min(ax, bx) - radius, min(ay, by) - radius,
                                     max(ax, bx) + radius, max(ay, by) + radius):
                x, y = grid_xy(node)
                if segment_distance(x, y, ax, ay, bx, by) < radius:
                    result.add(node)
    for footprint in board.GetFootprints():
        for pad in footprint.Pads():
            if pad.GetNetCode() == net_code or not pad.IsOnLayer(layer):
                continue
            box = pad.GetBoundingBox()
            result.update(nodes_in_box(mm(box.GetX()) - 0.23, mm(box.GetY()) - 0.23,
                                       mm(box.GetRight()) + 0.23, mm(box.GetBottom()) + 0.23))
    return result


def grid_xy(node):
    return (BOUNDS[0] + node[0] * GRID, BOUNDS[1] + node[1] * GRID)


def grid_node(xy):
    return (round((xy[0] - BOUNDS[0]) / GRID), round((xy[1] - BOUNDS[1]) / GRID))


def route(board, net, start, end, layer=pcbnew.B_Cu):
    start_node, end_node = grid_node(start), grid_node(end)
    open_set = [(0.0, start_node)]
    came_from = {}
    cost = {start_node: 0.0}
    directions = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
                  (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
    max_x = round((BOUNDS[2] - BOUNDS[0]) / GRID)
    max_y = round((BOUNDS[3] - BOUNDS[1]) / GRID)
    blocked_nodes = build_blocked(board, layer, net.GetNetCode())

    while open_set:
        _, current = heapq.heappop(open_set)
        if current == end_node:
            break
        for dx, dy, step_cost in directions:
            nxt = (current[0] + dx, current[1] + dy)
            if not (0 <= nxt[0] <= max_x and 0 <= nxt[1] <= max_y):
                continue
            if nxt not in (start_node, end_node):
                xy = grid_xy(nxt)
                endpoint_escape = min(math.hypot(xy[0] - start[0], xy[1] - start[1]),
                                      math.hypot(xy[0] - end[0], xy[1] - end[1])) < 0.55
                if nxt in blocked_nodes and not endpoint_escape:
                    continue
            new_cost = cost[current] + step_cost
            if new_cost >= cost.get(nxt, float("inf")):
                continue
            cost[nxt] = new_cost
            came_from[nxt] = current
            heuristic = math.hypot(end_node[0] - nxt[0], end_node[1] - nxt[1])
            heapq.heappush(open_set, (new_cost + heuristic, nxt))
    else:
        raise RuntimeError(f"no route for {net.GetNetname()}: {start} -> {end}")

    nodes = [end_node]
    while nodes[-1] != start_node:
        nodes.append(came_from[nodes[-1]])
    nodes.reverse()
    raw = [start] + [grid_xy(node) for node in nodes] + [end]

    # Collapse collinear grid steps before creating PCB segments.
    clean = [raw[0]]
    for candidate in raw[1:]:
        if len(clean) >= 2:
            a, b = clean[-2], clean[-1]
            cross = (b[0] - a[0]) * (candidate[1] - b[1]) - (b[1] - a[1]) * (candidate[0] - b[0])
            if abs(cross) < 1e-8:
                clean[-1] = candidate
                continue
        clean.append(candidate)
    for a, b in zip(clean, clean[1:]):
        if math.hypot(b[0] - a[0], b[1] - a[1]) > 0.001:
            add_track(board, net, layer, a, b)
    print(net.GetNetname(), len(clean) - 1, "segments")


def main():
    board = pcbnew.LoadBoard(str(SOURCE))
    nets = {net.GetNetname(): net for net in board.GetNetInfo().NetsByName().values()}

    # J2 is front-side only. Escape below the fine-pitch connector before the via.
    lcd_escape = (106.75, 56.0)
    add_track(board, nets["/LCD_SCS"], pcbnew.F_Cu, (106.75, 52.75), lcd_escape)
    lcd_rear = (144.5, 86.5)
    route(board, nets["/LCD_SCS"], lcd_escape, lcd_rear, layer=pcbnew.F_Cu)
    add_via(board, nets["/LCD_SCS"], lcd_rear)
    add_track(board, nets["/LCD_SCS"], pcbnew.B_Cu, lcd_rear, (147.825, 86.5))

    route(board, nets["/BAT_TS"], (134.675, 100.8), (141.5375, 102.25))
    bclk_start, bclk_end = (61.5, 92.5625), (139.5, 78.9)
    add_track(board, nets["/I2S_BCLK"], pcbnew.B_Cu, (63.25, 92.5625), bclk_start)
    add_via(board, nets["/I2S_BCLK"], bclk_start)
    route(board, nets["/I2S_BCLK"], bclk_start, bclk_end, layer=pcbnew.F_Cu)
    add_via(board, nets["/I2S_BCLK"], bclk_end)
    add_track(board, nets["/I2S_BCLK"], pcbnew.B_Cu, bclk_end, (142.25, 78.9))
    route(board, nets["/VSYS"], (126.6, 94.46), (132.6, 90.0))
    route(board, nets["/VSYS"], (132.6, 90.0), (142.8629, 97.2170))
    gauge_points = [(129.9, 106.2), (130.6, 106.2), (130.6, 108.0), (131.175, 108.0)]
    for a, b in zip(gauge_points, gauge_points[1:]):
        add_track(board, nets["/GAUGE_BIN"], pcbnew.B_Cu, a, b, width=0.15)

    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(OUTPUT), board)
    print(OUTPUT)


if __name__ == "__main__":
    main()
