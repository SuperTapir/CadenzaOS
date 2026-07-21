#!/usr/bin/env python3
"""Reroute signals around the ESP32-S3 module antenna copper keepout."""

import argparse
import heapq
import math
import os
import sys

import route_gauge_signals as rg

pcbnew = rg.pcbnew


KEEP_OUT = (141.75, 49.75, 166.00, 67.25)
ROUTES = (
    ("/PDM_CLK", (140.80, 68.00), pcbnew.B_Cu, (160.8017, 68.0864), pcbnew.B_Cu),
    ("/PDM_DATA", (140.20, 68.60), pcbnew.B_Cu, (158.6983, 71.5327), pcbnew.B_Cu),
    ("/LCD_SCLK", (138.6276, 56.4241), pcbnew.B_Cu, (152.6296, 70.4261), pcbnew.B_Cu),
    ("/LCD_SI", (132.0430, 51.1343), pcbnew.B_Cu, (152.2279, 71.3192), pcbnew.B_Cu),
    ("/LCD_SCS", (134.7119, 55.7985), pcbnew.B_Cu, (151.7730, 72.8596), pcbnew.B_Cu),
    ("/LCD_EXTCOMIN", (140.80, 54.2769), pcbnew.B_Cu, (153.4330, 67.5451), pcbnew.B_Cu),
    ("/LCD_DISP", (139.3965, 54.1592), pcbnew.B_Cu, (153.0313, 67.7940), pcbnew.B_Cu),
)


def inside(point, margin=0.0):
    return (
        KEEP_OUT[0] - margin <= point[0] <= KEEP_OUT[2] + margin
        and KEEP_OUT[1] - margin <= point[1] <= KEEP_OUT[3] + margin
    )


def segment_hits_keepout(a, b):
    return any(
        inside((a[0] + (b[0] - a[0]) * i / 200, a[1] + (b[1] - a[1]) * i / 200))
        for i in range(201)
    )


def add_track(board, net, layer, a, b, width=rg.WIDTH):
    track = pcbnew.PCB_TRACK(board)
    track.SetStart(pcbnew.VECTOR2I(rg.mm(a[0]), rg.mm(a[1])))
    track.SetEnd(pcbnew.VECTOR2I(rg.mm(b[0]), rg.mm(b[1])))
    track.SetWidth(rg.mm(width))
    track.SetLayer(layer)
    track.SetNet(net)
    board.Add(track)


def add_via(board, net, position):
    via = pcbnew.PCB_VIA(board)
    via.SetPosition(pcbnew.VECTOR2I(rg.mm(position[0]), rg.mm(position[1])))
    via.SetWidth(rg.mm(rg.VIA_SIZE))
    via.SetDrill(rg.mm(rg.VIA_DRILL))
    via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    via.SetNet(net)
    board.Add(via)


def remove_crossing_tracks(board):
    affected = {item[0] for item in ROUTES}
    removed = 0
    for item in list(board.GetTracks()):
        if item.GetNetname() not in affected:
            continue
        if item.Type() == pcbnew.PCB_VIA_T:
            if inside(rg.point_mm(item.GetPosition())):
                board.Remove(item)
                removed += 1
            continue
        if segment_hits_keepout(rg.point_mm(item.GetStart()), rg.point_mm(item.GetEnd())):
            board.Remove(item)
            removed += 1
    print(f"removed {removed} antenna-crossing tracks/vias")


def prepare_pdm_sources(board, nets):
    for net_name, source, original in (
        ("/LCD_EXTCOMIN", (140.80, 54.2769), (113.7517, 54.2769)),
        ("/PDM_CLK", (140.80, 65.5232), (67.6881, 65.5232)),
        ("/PDM_DATA", (140.20, 64.8404), (74.1467, 64.8404)),
    ):
        net = nets[net_name]
        add_track(board, net, pcbnew.F_Cu, original, source, 0.20)
        add_via(board, net, source)
        if net_name == "/PDM_CLK":
            add_track(board, net, pcbnew.B_Cu, source, (140.80, 68.00))
        elif net_name == "/PDM_DATA":
            add_track(board, net, pcbnew.B_Cu, source, (140.20, 68.60))


def obstacle_index(items):
    index = {}
    for item in items:
        if item[0] == "rect":
            x0, y0, x1, y1 = item[1:]
        elif item[0] == "circle":
            x0, y0, x1, y1 = item[1] - item[3], item[2] - item[3], item[1] + item[3], item[2] + item[3]
        else:
            radius = item[5]
            x0, y0 = min(item[1], item[3]) - radius, min(item[2], item[4]) - radius
            x1, y1 = max(item[1], item[3]) + radius, max(item[2], item[4]) + radius
        for ix in range(math.floor(x0), math.floor(x1) + 1):
            for iy in range(math.floor(y0), math.floor(y1) + 1):
                index.setdefault((ix, iy), []).append(item)
    return index


def clear_indexed(a, b, index):
    nearby = []
    seen = set()
    for ix in range(math.floor(min(a[0], b[0])) - 1, math.floor(max(a[0], b[0])) + 2):
        for iy in range(math.floor(min(a[1], b[1])) - 1, math.floor(max(a[1], b[1])) + 2):
            for item in index.get((ix, iy), ()):
                if item not in seen:
                    seen.add(item)
                    nearby.append(item)
    return rg.clear_segment(a, b, nearby)


def route_points(board, nets, net_name, start_exact, start_layer, goal_exact, goal_layer):
    net = nets[net_name]
    start = (rg.snap(start_exact[0]), rg.snap(start_exact[1]))
    goal = (rg.snap(goal_exact[0]), rg.snap(goal_exact[1]))
    margin = 10.0 if net_name in {"/LCD_EXTCOMIN", "/LCD_DISP", "/PDM_CLK", "/PDM_DATA"} else 4.0
    min_x = rg.snap(min(start_exact[0], goal_exact[0]) - margin)
    max_x = rg.snap(max(start_exact[0], goal_exact[0]) + margin)
    min_y = rg.snap(min(start_exact[1], goal_exact[1]) - margin)
    max_y = rg.snap(max(start_exact[1], goal_exact[1]) + margin)
    window = (min_x * rg.GRID, min_y * rg.GRID, max_x * rg.GRID, max_y * rg.GRID)
    layers = (pcbnew.B_Cu, pcbnew.F_Cu)
    blocked = {
        layer: rg.obstacles(
            board, net.GetNetCode(), set(), window, layer, rg.WIDTH / 2 + rg.CLEARANCE
        )
        for layer in layers
    }
    via_blocked = {
        layer: rg.obstacles(
            board, net.GetNetCode(), set(), window, layer, rg.VIA_SIZE / 2 + rg.CLEARANCE
        )
        for layer in layers
    }
    blocked_rect = (
        "rect",
        KEEP_OUT[0] - rg.WIDTH / 2,
        KEEP_OUT[1] - rg.WIDTH / 2,
        KEEP_OUT[2] + rg.WIDTH / 2,
        KEEP_OUT[3] + rg.WIDTH / 2,
    )
    via_rect = (
        "rect",
        KEEP_OUT[0] - rg.VIA_SIZE / 2,
        KEEP_OUT[1] - rg.VIA_SIZE / 2,
        KEEP_OUT[2] + rg.VIA_SIZE / 2,
        KEEP_OUT[3] + rg.VIA_SIZE / 2,
    )
    for layer in layers:
        blocked[layer].append(blocked_rect)
        via_blocked[layer].append(via_rect)

    start_state, goal_state = (*start, start_layer), (*goal, goal_layer)
    queue = [(0.0, 0.0, start_state)]
    costs = {start_state: 0.0}
    parent = {}
    directions = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1))
    while queue:
        _, current_cost, current = heapq.heappop(queue)
        if current == goal_state:
            break
        if current_cost != costs.get(current):
            continue
        for dx, dy in directions:
            nxt = current[0] + dx, current[1] + dy, current[2]
            if not (min_x <= nxt[0] <= max_x and min_y <= nxt[1] <= max_y):
                continue
            if not rg.clear_segment(rg.unsnap(current), rg.unsnap(nxt), blocked[current[2]]):
                continue
            new_cost = current_cost + (math.sqrt(2) if dx and dy else 1.0)
            if new_cost >= costs.get(nxt, float("inf")):
                continue
            costs[nxt], parent[nxt] = new_cost, current
            heuristic = math.hypot(goal[0] - nxt[0], goal[1] - nxt[1])
            heapq.heappush(queue, (new_cost + heuristic, new_cost, nxt))
        point = rg.unsnap(current)
        other = pcbnew.F_Cu if current[2] == pcbnew.B_Cu else pcbnew.B_Cu
        via_state = (current[0], current[1], other)
        far = (
            math.hypot(current[0] - start[0], current[1] - start[1]) >= 4
            and math.hypot(current[0] - goal[0], current[1] - goal[1]) >= 4
        )
        if (
            far
            and rg.clear_segment(point, point, via_blocked[current[2]])
            and rg.clear_segment(point, point, via_blocked[other])
        ):
            new_cost = current_cost + 10.0
            if new_cost < costs.get(via_state, float("inf")):
                costs[via_state], parent[via_state] = new_cost, current
                heuristic = math.hypot(goal[0] - current[0], goal[1] - current[1])
                heapq.heappush(queue, (new_cost + heuristic, new_cost, via_state))
    else:
        raise SystemExit(f"no antenna-safe path for {net_name}")

    nodes = [goal_state]
    while nodes[-1] != start_state:
        nodes.append(parent[nodes[-1]])
    nodes.reverse()
    points = [(start_exact, start_layer)] + [(rg.unsnap(node), node[2]) for node in nodes] + [(goal_exact, goal_layer)]
    compact = []
    for value in points:
        point, layer = value
        if compact and math.dist(point, compact[-1][0]) < 1e-6 and layer == compact[-1][1]:
            continue
        compact.append(value)
        while len(compact) >= 3:
            (a, la), (b, lb), (c, lc) = compact[-3:]
            if la != lb or lb != lc:
                break
            cross = (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])
            if abs(cross) > 1e-8:
                break
            compact.pop(-2)
    vias = 0
    for (a, la), (b, lb) in zip(compact, compact[1:]):
        if la != lb:
            add_via(board, net, a)
            vias += 1
        else:
            add_track(board, net, la, a, b)
    print(f"{net_name}: {len(compact) - 1 - vias} segments, {vias} vias")


def add_keepout(board):
    outline = pcbnew.SHAPE_POLY_SET()
    outline.NewOutline()
    for x, y in (
        (KEEP_OUT[0], KEEP_OUT[1]),
        (KEEP_OUT[2], KEEP_OUT[1]),
        (KEEP_OUT[2], KEEP_OUT[3]),
        (KEEP_OUT[0], KEEP_OUT[3]),
    ):
        outline.Append(rg.mm(x), rg.mm(y))
    zone = pcbnew.ZONE(board)
    zone.SetIsRuleArea(True)
    zone.SetDoNotAllowTracks(True)
    zone.SetDoNotAllowVias(True)
    zone.SetDoNotAllowZoneFills(True)
    zone.SetLayerSet(pcbnew.LSET.AllCuMask())
    zone.SetOutline(outline)
    board.Add(zone)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    parser.add_argument("--prepared", action="store_true")
    args = parser.parse_args()
    board = pcbnew.LoadBoard(args.input)
    if not args.prepared:
        remove_crossing_tracks(board)
        stage = args.output + ".stage.kicad_pcb"
        pcbnew.SaveBoard(stage, board)
        os.execv(sys.executable, [sys.executable, __file__, stage, args.output, "--prepared"])
    stage_input = args.input if args.input.endswith(".stage.kicad_pcb") else None
    nets = {net_name: board.FindNet(net_name) for net_name, *_ in ROUTES}
    prepare_pdm_sources(board, nets)
    for route in ROUTES:
        route_points(board, nets, *route)
    pcbnew.SaveBoard(args.output, board)
    if stage_input and os.path.exists(stage_input):
        os.unlink(stage_input)


if __name__ == "__main__":
    main()
