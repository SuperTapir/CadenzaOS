#!/usr/bin/env python3
"""Cut away the PCB under the ESP32-S3-WROOM-1 antenna."""

import argparse
import math

import pcbnew


MM = 1_000_000


def mm(value):
    return int(round(value * MM))


def point(point):
    return point.x / MM, point.y / MM


def same_segment(item, a, b):
    ends = (point(item.GetStart()), point(item.GetEnd()))
    return (
        math.dist(ends[0], a) < 0.01 and math.dist(ends[1], b) < 0.01
    ) or (
        math.dist(ends[0], b) < 0.01 and math.dist(ends[1], a) < 0.01
    )


def add_edge(board, a, b):
    edge = pcbnew.PCB_SHAPE(board)
    edge.SetShape(pcbnew.SHAPE_T_SEGMENT)
    edge.SetStart(pcbnew.VECTOR2I(mm(a[0]), mm(a[1])))
    edge.SetEnd(pcbnew.VECTOR2I(mm(b[0]), mm(b[1])))
    edge.SetLayer(pcbnew.Edge_Cuts)
    edge.SetWidth(mm(0.05))
    board.Add(edge)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    board = pcbnew.LoadBoard(args.input)
    old_edges = (
        ((56.0, 50.0), (160.0, 50.0)),
        ((160.0, 50.0), (166.0, 56.0)),
        ((166.0, 56.0), (166.0, 108.0)),
    )
    matches = []
    for item in board.GetDrawings():
        if item.GetLayer() != pcbnew.Edge_Cuts:
            continue
        if any(same_segment(item, a, b) for a, b in old_edges):
            matches.append(item)
    if len(matches) != 3:
        raise SystemExit(f"expected three upper-right board edges, found {len(matches)}")
    for item in matches:
        board.Remove(item)
    new_edges = (
        ((56.0, 50.0), (141.75, 50.0)),
        ((141.75, 50.0), (141.75, 67.5)),
        ((141.75, 67.5), (166.0, 67.5)),
        ((166.0, 67.5), (166.0, 108.0)),
    )
    for a, b in new_edges:
        add_edge(board, a, b)
    pcbnew.SaveBoard(args.output, board)
    print("antenna cutout: x=141.75..166.00 mm, y=50.00..67.50 mm")


if __name__ == "__main__":
    main()
