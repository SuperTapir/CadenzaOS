#!/usr/bin/env python3
"""Import Freerouting Specctra SES routing into a clean KiCad PCB.

KiCad 10's Python ImportSpecctraSES currently rejects the SES emitted by
Freerouting 1.9 even though the network_out section is valid.  This importer
intentionally consumes only network_out wires and vias, leaving placement,
design rules, existing locked routes, and zones untouched.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

import pcbnew


TOKEN_RE = re.compile(r'\s*(?:([()])|"((?:\\.|[^"\\])*)"|([^\s()]+))')


def parse_sexpr(text: str):
    tokens = []
    for match in TOKEN_RE.finditer(text):
        paren, quoted, bare = match.groups()
        if paren:
            tokens.append(paren)
        elif quoted is not None:
            tokens.append(bytes(quoted, "utf-8").decode("unicode_escape"))
        else:
            tokens.append(bare)

    stack = []
    root = None
    for token in tokens:
        if token == "(":
            node = []
            if stack:
                stack[-1].append(node)
            stack.append(node)
        elif token == ")":
            if not stack:
                raise ValueError("unexpected closing parenthesis")
            root = stack.pop()
        else:
            if not stack:
                raise ValueError("atom outside list")
            stack[-1].append(token)
    if stack:
        raise ValueError("unterminated S-expression")
    return root


def find_first(node, name: str):
    if isinstance(node, list):
        if node and node[0] == name:
            return node
        for child in node:
            found = find_first(child, name)
            if found is not None:
                return found
    return None


def coord(value: str) -> int:
    # SES declares (resolution um 10): one coordinate unit is 0.1 um.
    return pcbnew.FromMM(int(value) / 10000.0)


def board_point(x: str, y: str) -> pcbnew.VECTOR2I:
    # Specctra's positive Y axis points up; KiCad's points down.
    return pcbnew.VECTOR2I(coord(x), -coord(y))


def parse_via_size(name: str) -> tuple[float, float]:
    match = re.search(r"_(\d+):(\d+)_um$", name)
    if not match:
        raise ValueError(f"unsupported via padstack: {name}")
    return int(match.group(1)) / 1000.0, int(match.group(2)) / 1000.0


def import_routes(source: Path, session: Path, output: Path) -> tuple[int, int]:
    board = pcbnew.LoadBoard(str(source))
    tree = parse_sexpr(session.read_text(encoding="utf-8"))
    network_out = find_first(tree, "network_out")
    if network_out is None:
        raise ValueError("SES has no network_out section")

    net_lookup = {net.GetNetname(): net for net in board.GetNetInfo().NetsByName().values()}
    wire_count = 0
    via_count = 0

    for net_node in network_out[1:]:
        if not isinstance(net_node, list) or len(net_node) < 2 or net_node[0] != "net":
            continue
        net_name = net_node[1]
        if net_name not in net_lookup:
            raise KeyError(f"SES net is absent from KiCad board: {net_name}")
        net = net_lookup[net_name]

        for item in net_node[2:]:
            if not isinstance(item, list) or not item:
                continue
            if item[0] == "wire":
                path = next((child for child in item[1:] if isinstance(child, list) and child and child[0] == "path"), None)
                if path is None or len(path) < 7 or (len(path) - 3) % 2:
                    raise ValueError(f"malformed wire on {net_name}")
                layer_name = path[1]
                layer_id = board.GetLayerID(layer_name)
                if layer_id < 0:
                    raise KeyError(f"SES layer is absent from KiCad board: {layer_name}")
                width = coord(path[2])
                points = [board_point(path[i], path[i + 1]) for i in range(3, len(path), 2)]
                for start, end in zip(points, points[1:]):
                    track = pcbnew.PCB_TRACK(board)
                    track.SetStart(start)
                    track.SetEnd(end)
                    track.SetWidth(width)
                    track.SetLayer(layer_id)
                    track.SetNet(net)
                    board.Add(track)
                    wire_count += 1
            elif item[0] == "via":
                if len(item) < 4:
                    raise ValueError(f"malformed via on {net_name}")
                diameter_mm, drill_mm = parse_via_size(item[1])
                via = pcbnew.PCB_VIA(board)
                via.SetPosition(board_point(item[2], item[3]))
                via.SetWidth(pcbnew.FromMM(diameter_mm))
                via.SetDrill(pcbnew.FromMM(drill_mm))
                via.SetViaType(pcbnew.VIATYPE_THROUGH)
                via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
                via.SetNet(net)
                board.Add(via)
                via_count += 1

    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    pcbnew.SaveBoard(str(output), board)
    return wire_count, via_count


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("session", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    wires, vias = import_routes(args.source, args.session, args.output)
    print(f"Imported {wires} track segments and {vias} vias into {args.output}")


if __name__ == "__main__":
    main()
