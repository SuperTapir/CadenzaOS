#!/usr/bin/env python3
"""Route the six remaining BQ27441 local signals without disturbing Pass 1."""

import argparse
import heapq
import math

import pcbnew


MM = 1_000_000
GRID = 0.20
WIDTH = 0.15
CLEARANCE = 0.15
VIA_SIZE = 0.50
VIA_DRILL = 0.20

ROUTES = (
    ("/GAUGE_BIN", "U7", "R72"),
    ("/GAUGE_SRN", "U7", "R73"),
    ("/GAUGE_SRP", "U7", "R73"),
    ("/VBAT_SENSE", "U7", "C70"),
    ("/I2C_SCL", "U7", "R71"),
    ("/I2C_SDA", "U7", "R70"),
)

SOURCE_FANOUTS = {
    "/GAUGE_SRN": ([(123.8, 103.0)], True),
    "/GAUGE_BIN": ([(123.8, 103.8)], True),
    "/GAUGE_SRP": ([(123.8, 104.6)], True),
    "/I2C_SDA": ([(118.2, 105.0)], False),
    "/I2C_SCL": ([(118.9, 103.4), (118.35, 103.2), (117.8, 103.2)], False),
    "/VBAT_SENSE": ([(118.9, 103.0), (118.5, 102.6), (117.8, 102.6)], False),
}


def mm(value):
    return int(round(value * MM))


def point_mm(point):
    return point.x / MM, point.y / MM


def pad_for(board, reference, net_name):
    footprint = board.FindFootprintByReference(reference)
    if footprint is None:
        raise SystemExit(f"missing footprint {reference}")
    matches = [pad for pad in footprint.Pads() if pad.GetNetname() == net_name]
    if len(matches) != 1:
        raise SystemExit(
            f"expected one {reference} pad on {net_name}, found {len(matches)}"
        )
    return matches[0]


def reroute_usb_dp(board):
    net = board.FindNet("/USB_D+")
    candidates = []
    for item in board.GetTracks():
        if item.Type() != pcbnew.PCB_TRACE_T or item.GetLayer() != pcbnew.B_Cu:
            continue
        if item.GetNetCode() != net.GetNetCode():
            continue
        ends = sorted((point_mm(item.GetStart()), point_mm(item.GetEnd())))
        if math.dist(ends[0], (122.6743, 101.95)) < 0.01 and math.dist(ends[1], (126.7485, 106.0243)) < 0.01:
            candidates.append(item)
    if len(candidates) != 1:
        raise SystemExit(f"expected one local /USB_D+ diagonal, found {len(candidates)}")
    board.Remove(candidates[0])
    points = ((122.6743, 101.95), (124.8, 101.95), (126.7485, 103.8985), (126.7485, 106.0243))
    for start, end in zip(points, points[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(mm(start[0]), mm(start[1])))
        track.SetEnd(pcbnew.VECTOR2I(mm(end[0]), mm(end[1])))
        track.SetWidth(mm(0.20))
        track.SetLayer(pcbnew.B_Cu)
        track.SetNet(net)
        board.Add(track)


def point_segment_distance(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (ax + t * dx), py - (ay + t * dy))


def segments_distance(a, b, c, d):
    def orient(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

    def on_segment(p, q, r):
        return (
            min(p[0], r[0]) - 1e-9 <= q[0] <= max(p[0], r[0]) + 1e-9
            and min(p[1], r[1]) - 1e-9 <= q[1] <= max(p[1], r[1]) + 1e-9
        )

    o1, o2 = orient(a, b, c), orient(a, b, d)
    o3, o4 = orient(c, d, a), orient(c, d, b)
    proper_crossing = o1 * o2 < 0 and o3 * o4 < 0
    endpoint_touch = (
        (abs(o1) < 1e-9 and on_segment(a, c, b))
        or (abs(o2) < 1e-9 and on_segment(a, d, b))
        or (abs(o3) < 1e-9 and on_segment(c, a, d))
        or (abs(o4) < 1e-9 and on_segment(c, b, d))
    )
    if proper_crossing or endpoint_touch:
        return 0.0
    return min(
        point_segment_distance(*a, *c, *d),
        point_segment_distance(*b, *c, *d),
        point_segment_distance(*c, *a, *b),
        point_segment_distance(*d, *a, *b),
    )


def segment_hits_rect(a, b, rect):
    x0, y0, x1, y1 = rect
    if x0 <= a[0] <= x1 and y0 <= a[1] <= y1:
        return True
    if x0 <= b[0] <= x1 and y0 <= b[1] <= y1:
        return True
    corners = ((x0, y0), (x1, y0), (x1, y1), (x0, y1))
    return any(
        segments_distance(a, b, corners[i], corners[(i + 1) % 4]) < 1e-6
        for i in range(4)
    )


def obstacles(board, net_code, ignored_references, window, layer, conductor_radius):
    result = []
    wx0, wy0, wx1, wy1 = window
    for footprint in board.GetFootprints():
        if footprint.GetReference() in ignored_references:
            continue
        for pad in footprint.Pads():
            if pad.GetNetCode() == net_code or not pad.IsOnLayer(layer):
                continue
            box = pad.GetBoundingBox()
            if (
                box.GetRight() / MM < wx0
                or box.GetX() / MM > wx1
                or box.GetBottom() / MM < wy0
                or box.GetY() / MM > wy1
            ):
                continue
            result.append((
                "rect",
                box.GetX() / MM - conductor_radius,
                box.GetY() / MM - conductor_radius,
                box.GetRight() / MM + conductor_radius,
                box.GetBottom() / MM + conductor_radius,
            ))
    for item in board.GetTracks():
        if item.GetNetCode() == net_code:
            continue
        if item.Type() == pcbnew.PCB_VIA_T:
            pos = point_mm(item.GetPosition())
            if not (wx0 <= pos[0] <= wx1 and wy0 <= pos[1] <= wy1):
                continue
            radius = item.GetWidth(layer) / MM / 2 + conductor_radius
            result.append(("circle", pos[0], pos[1], radius))
        elif item.GetLayer() == layer:
            start, end = point_mm(item.GetStart()), point_mm(item.GetEnd())
            if (
                max(start[0], end[0]) < wx0
                or min(start[0], end[0]) > wx1
                or max(start[1], end[1]) < wy0
                or min(start[1], end[1]) > wy1
            ):
                continue
            radius = item.GetWidth() / MM / 2 + conductor_radius
            result.append(("segment", *start, *end, radius))
    return result


def clear_segment(a, b, blocked):
    for item in blocked:
        if item[0] == "rect" and segment_hits_rect(a, b, item[1:]):
            return False
        if item[0] == "circle" and point_segment_distance(item[1], item[2], *a, *b) < item[3] - 1e-6:
            return False
        if item[0] == "segment" and segments_distance(a, b, item[1:3], item[3:5]) < item[5] - 1e-6:
            return False
    return True


def snap(value):
    return int(round(value / GRID))


def unsnap(node):
    return node[0] * GRID, node[1] * GRID


def route(board, net_name, source, target):
    net = board.FindNet(net_name)
    if net is None:
        raise SystemExit(f"missing net {net_name}")
    source_exact, target_exact = point_mm(source.GetPosition()), point_mm(target.GetPosition())
    goal_exact = target_exact
    if net_name == "/VBAT_SENSE":
        goal_exact = (108.0, 100.8)
        lead = pcbnew.PCB_TRACK(board)
        lead.SetStart(pcbnew.VECTOR2I(mm(goal_exact[0]), mm(goal_exact[1])))
        lead.SetEnd(target.GetPosition())
        lead.SetWidth(mm(WIDTH))
        lead.SetLayer(pcbnew.B_Cu)
        lead.SetNet(net)
        board.Add(lead)
    start_exact = source_exact
    start_layer = pcbnew.B_Cu
    prefanned_vias = 0
    fanout_points, fanout_via = SOURCE_FANOUTS[net_name]
    previous = source_exact
    for fanout_point in fanout_points:
        lead = pcbnew.PCB_TRACK(board)
        lead.SetStart(pcbnew.VECTOR2I(mm(previous[0]), mm(previous[1])))
        lead.SetEnd(pcbnew.VECTOR2I(mm(fanout_point[0]), mm(fanout_point[1])))
        lead.SetWidth(mm(WIDTH))
        lead.SetLayer(pcbnew.B_Cu)
        lead.SetNet(net)
        board.Add(lead)
        previous = fanout_point
    start_exact = fanout_points[-1]
    if fanout_via:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(pcbnew.VECTOR2I(mm(start_exact[0]), mm(start_exact[1])))
        via.SetWidth(mm(VIA_SIZE))
        via.SetDrill(mm(VIA_DRILL))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetNet(net)
        board.Add(via)
        start_layer = pcbnew.F_Cu
        prefanned_vias = 1
    start, goal = (snap(start_exact[0]), snap(start_exact[1])), (snap(goal_exact[0]), snap(goal_exact[1]))
    margin = 8.0
    min_x = snap(min(start_exact[0], goal_exact[0]) - margin)
    max_x = snap(max(start_exact[0], goal_exact[0]) + margin)
    min_y = snap(min(start_exact[1], goal_exact[1]) - margin)
    max_y = snap(max(start_exact[1], goal_exact[1]) + margin)
    ignored = set() if net_name == "/VBAT_SENSE" else {target.GetParentFootprint().GetReference()}
    window = (min_x * GRID, min_y * GRID, max_x * GRID, max_y * GRID)
    layers = (pcbnew.B_Cu, pcbnew.F_Cu)
    blocked = {
        layer: obstacles(
            board,
            net.GetNetCode(),
            ignored,
            window,
            layer,
            WIDTH / 2 + CLEARANCE,
        )
        for layer in layers
    }
    via_blocked = {
        layer: obstacles(
            board,
            net.GetNetCode(),
            ignored,
            window,
            layer,
            VIA_SIZE / 2 + CLEARANCE,
        )
        for layer in layers
    }
    start_state = (*start, start_layer)
    goal_state = (*goal, pcbnew.B_Cu)
    queue = [(0.0, 0.0, start_state)]
    cost = {start_state: 0.0}
    parent = {}
    directions = ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (1, -1), (-1, 1), (-1, -1))
    while queue:
        _, current_cost, current = heapq.heappop(queue)
        if current == goal_state:
            break
        if current_cost != cost.get(current):
            continue
        for dx, dy in directions:
            nxt = current[0] + dx, current[1] + dy, current[2]
            if not (min_x <= nxt[0] <= max_x and min_y <= nxt[1] <= max_y):
                continue
            if not clear_segment(unsnap(current), unsnap(nxt), blocked[current[2]]):
                continue
            step = math.sqrt(2) if dx and dy else 1.0
            new_cost = current_cost + step
            if new_cost >= cost.get(nxt, float("inf")):
                continue
            cost[nxt] = new_cost
            parent[nxt] = current
            heuristic = math.hypot(goal[0] - nxt[0], goal[1] - nxt[1])
            heapq.heappush(queue, (new_cost + heuristic, new_cost, nxt))
        point = unsnap(current)
        other_layer = pcbnew.F_Cu if current[2] == pcbnew.B_Cu else pcbnew.B_Cu
        far_from_ends = (
            math.hypot(current[0] - start[0], current[1] - start[1]) >= 4
            and math.hypot(current[0] - goal[0], current[1] - goal[1]) >= 4
        )
        via_state = (current[0], current[1], other_layer)
        if (
            far_from_ends
            and clear_segment(point, point, via_blocked[current[2]])
            and clear_segment(point, point, via_blocked[other_layer])
        ):
            new_cost = current_cost + 10.0
            if new_cost < cost.get(via_state, float("inf")):
                cost[via_state] = new_cost
                parent[via_state] = current
                heuristic = math.hypot(goal[0] - current[0], goal[1] - current[1])
                heapq.heappush(queue, (new_cost + heuristic, new_cost, via_state))
    else:
        exits = []
        for dx, dy in directions:
            nxt = start[0] + dx, start[1] + dy, start_layer
            exits.append((nxt, clear_segment(unsnap(start_state), unsnap(nxt), blocked[start_layer])))
        raise SystemExit(
            f"no clear B.Cu path for {net_name}; start={unsnap(start)} "
            f"goal={unsnap(goal)} obstacles={len(blocked[pcbnew.B_Cu])} exits={exits}"
        )

    nodes = [goal_state]
    while nodes[-1] != start_state:
        nodes.append(parent[nodes[-1]])
    nodes.reverse()
    points = [(start_exact, start_layer)] + [(unsnap(node), node[2]) for node in nodes] + [(goal_exact, pcbnew.B_Cu)]
    compact = []
    for point, layer in points:
        if compact and math.dist(point, compact[-1][0]) < 1e-6 and layer == compact[-1][1]:
            continue
        compact.append((point, layer))
        while len(compact) >= 3:
            (a, la), (b, lb), (c, lc) = compact[-3:]
            if la != lb or lb != lc:
                break
            if abs((b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])) > 1e-8:
                break
            compact.pop(-2)
    for (a, layer_a), (b, layer_b) in zip(compact, compact[1:]):
        if layer_a != layer_b:
            via = pcbnew.PCB_VIA(board)
            via.SetPosition(pcbnew.VECTOR2I(mm(a[0]), mm(a[1])))
            via.SetWidth(mm(VIA_SIZE))
            via.SetDrill(mm(VIA_DRILL))
            via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            via.SetNet(net)
            board.Add(via)
            continue
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(mm(a[0]), mm(a[1])))
        track.SetEnd(pcbnew.VECTOR2I(mm(b[0]), mm(b[1])))
        track.SetWidth(mm(WIDTH))
        track.SetLayer(layer_a)
        track.SetNet(net)
        board.Add(track)
    vias = prefanned_vias + sum(1 for a, b in zip(compact, compact[1:]) if a[1] != b[1])
    print(f"{net_name}: {source.GetParentFootprint().GetReference()} -> {target.GetParentFootprint().GetReference()}, {len(compact) - 1 - vias} segments, {vias} vias")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    board = pcbnew.LoadBoard(args.input)
    reroute_usb_dp(board)
    for net_name, source_ref, target_ref in ROUTES:
        route(
            board,
            net_name,
            pad_for(board, source_ref, net_name),
            pad_for(board, target_ref, net_name),
        )
    pcbnew.SaveBoard(args.output, board)


if __name__ == "__main__":
    main()
