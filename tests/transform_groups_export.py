"""Run with: python tests/transform_groups_export.py <SERE.exe> [output-dir]."""
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2] if len(sys.argv) > 2 else "out/group-tests").resolve()
out.mkdir(parents=True, exist_ok=True)


def node(id, name="Transform 2", **fields):
    return dict(Id=id, Name=name, Category="Transform", PosX=0.0, PosY=0.0, **fields)


def group(id, members):
    return node(id, "Transform Group", Label=f"Group {id}",
                Members=[dict(Node=m, Pin="Out") for m in members])


def link(a, output, b, input):
    return dict(LeftNode=a, LeftPin=output, RightNode=b, RightPin=input)


def run(name, nodes, links=(), error=None):
    graph = out / f"{name}.json"
    package = out / f"{name}.ruip"
    graph.write_text(json.dumps(dict(Nodes=nodes, Links=list(links))), encoding="utf-8")
    if package.exists():
        package.unlink()
    result = subprocess.run([str(exe), "--export-graph", str(graph), str(package),
                             "--size", "1920", "1080"], capture_output=True, text=True, timeout=30)
    if error:
        assert result.returncode != 0, (name, result.stdout, result.stderr)
        assert error in result.stdout + result.stderr, (name, result.stdout, result.stderr)
        assert not package.exists(), name
        return
    assert result.returncode == 0, (name, result.stdout, result.stderr)
    data = package.read_bytes()
    assert struct.unpack_from("<I", data, 68)[0] == 0, "unused defaultStringsSize must be initialized"
    # Arguments carry their names in a blob after the cluster; graphs without arguments skip it.
    names_size = struct.unpack_from("<I", data, 56)[0]
    names_offset = struct.unpack_from("<Q", data, 72)[0]
    assert (names_size == 0) == (names_offset == 0), (names_size, names_offset)
    assert names_offset + names_size <= len(data), (names_offset, names_size, len(data))
    count = struct.unpack_from("<H", data, 48)[0]
    offset = struct.unpack_from("<Q", data, 120)[0]
    commands = data[offset:offset + count]
    source = package.with_suffix(".cpp").read_text()
    assert "template<" not in source and "sereRebaseBounds" not in source and "ruiUnk5" not in source
    cursor, index, records, applies = 0, 3, [], []
    boundaries = set()
    while cursor < len(commands):
        type = commands[cursor]
        if type == 0:
            records.append((index, type)); index += 1; cursor += 1
        elif type in (*range(1, 11), 13):
            assert commands[cursor + 1] == 1
            records.append((index, type)); index += 1
            cursor += 4 if type == 1 else 32 if type == 10 else 22 if 7 <= type <= 9 else 12
        elif type == 11:
            cursor += 10
        elif type == 12:
            applies.append(struct.unpack_from("<HHH", commands, cursor + 2))
            cursor += 8
        else:
            raise AssertionError((name, cursor, type))
        boundaries.add(cursor)
    assert cursor == len(commands)
    barriers = list(map(int, re.findall(r"funcs->executeTransform\(inst,(\d+)\)", source)))
    # executeTransform is a do/while: repeated or empty barriers are invalid.
    assert barriers == sorted(set(barriers)) and all(b in boundaries for b in barriers), (name, barriers)
    assert barriers[-1] == len(commands)
    assignments = re.findall(r"transformSize\[(\d+)\] =", source)
    assert len(assignments) == len(set(assignments)), (name, "transform size rewritten", source)
    assert records[:len(applies)] == [(3 + i, 0) for i in range(len(applies))]
    if applies:
        assert commands[-8 * len(applies):][::8] == bytes([12]) * len(applies)
    for group_index, (frame, begin, end) in enumerate(applies):
        assert (begin, end) == (3 + group_index, 4 + group_index), (name, applies)
        assert frame >= end, (name, "type 12 must reference a later placement", applies)
        assert (frame, 4) in records

    return commands, records, applies, source


base = [node(1), node(2), node(3), group(10, [1, 3])]
commands, records, applies, source = run("flat", base)
assert commands[0] == 0
assert applies == [(7, 3, 4)], applies
assert "funcs->rebaseRuiTransformBounds(inst,5,7)" in source
assert len(records) == 5

# Canvas/node-array order is not membership or export order.
other = run("reordered", list(reversed(base)))
assert commands == other[0]

commands, records, applies, source = run("two_groups", [node(1), node(2), group(10, [1]), group(11, [2])])
assert commands[:2] == b"\x00\x00"
assert applies == [(6, 3, 4), (8, 4, 5)], applies
assert commands[-16:-8] == struct.pack("<BBHHH", 12, 1, 6, 3, 4)
assert commands[-8:] == struct.pack("<BBHHH", 12, 1, 8, 4, 5)

rooted = run("content_root", [node(1, "Transform 4"), group(10, [1])],
             [link(10, "Content Root", 1, "Parent")])
assert struct.unpack_from("<H", rooted[0], 3)[0] == 3
assert rooted[2] == [(5, 3, 4)]
assert [r[1] for r in rooted[1]] == [0, 4, 4]
assert len(re.findall(r"executeTransform", rooted[3])) == 2
run("frame_consumer", [node(1), group(10, [1]), node(20, "Copy Transform")],
    [link(10, "Frame", 20, "Source")])
run("many_members", [node(i) for i in range(1, 81)] + [group(100, list(range(1, 81)))])
run("copy_member", [node(1, "Copy Transform"), group(10, [1])])
commands, records, applies, source = run("empty_roots", [group(10, []), group(11, [])])
assert commands[:2] == b"\x00\x00"
assert applies == [(5, 3, 4), (6, 4, 5)], applies
assert "rebaseRuiTransformBounds(inst,3,4)" in source
assert "rebaseRuiTransformBounds(inst,4,5)" in source
assert "funcs->executeTransform(inst,2);" in source
assert len(re.findall(r"executeTransform", source)) == 3
root_size = group(10, [])
root_size["InputPins"] = {"Root Size": {"X": 200.0, "Y": 200.0, "Z": 40.0, "W": 40.0},
                           "Position": {"X": 5.059426, "Y": 0.5},
                           "Pivot": {"X": 0.5, "Y": 0.5}}
commands, _, applies, source = run("sized_root", [root_size])
assert "transformSize[3] = _mm_set_ps(40,40,200,200)" in source
assert applies == [(4, 3, 4)]
assert commands[1:3] == b"\x04\x01"
assert commands[-8:] == struct.pack("<BBHHH", 12, 1, 4, 3, 4)
commands, _, _, _ = run("builtin_parent", [node(1, "Transform 4"),
    node(2, "Built-in Transform")], [link(2, "0", 1, "Parent")])
assert struct.unpack_from("<H", commands, 2)[0] == 0
run("missing", [group(10, [99])], error="deleted")
run("overlap", [node(1), group(10, [1]), group(11, [1])], error="more than one group")
run("feedback", [node(1), group(10, [1])], [link(10, "Content Size", 1, "Size")], error="cannot depend on group outputs")
run("escaping_child", [node(1), node(2), group(10, [1])], [link(1, "Out", 2, "Parent")], error="Add dependent transforms")
run("frame_feedback", [node(1), node(2, "Copy Transform"), group(10, [1])],
    [link(10, "Frame", 2, "Source"), link(2, "Out", 10, "Parent")], error="Placement cannot depend")
# Retail gamestate_info_ffa has two early root records rebased against later frames.
# Keep this projection headless: rendering its text nodes requires a font atlas.
fixture = json.loads((Path(__file__).resolve().parents[1] / "examples" /
                      "gamestate_info_ffa.json").read_text(encoding="utf-8"))
fixture_ids = {900, 1003, 1004, *range(1005, 1011), *range(1013, 1019),
               *range(4005, 4011)}
fixture_nodes = [n for n in fixture["Nodes"] if n["Id"] in fixture_ids]
fixture_links = [l for l in fixture["Links"]
                 if l["LeftNode"] in fixture_ids and l["RightNode"] in fixture_ids]
fixture_commands, fixture_records, fixture_applies, fixture_source = run(
    "gamestate_info_ffa_transform12", fixture_nodes, fixture_links)
assert fixture_records[:2] == [(3, 0), (4, 0)]
assert fixture_applies == [(11, 3, 4), (12, 4, 5)]
assert fixture_records == [(3, 0), (4, 0)] + [(i, 4) for i in range(5, 19)]
assert "transformSize[16]" in fixture_source and "transformSize[17]" in fixture_source
assert fixture_commands[-16:] == (struct.pack("<BBHHH", 12, 1, 11, 3, 4) +
                                  struct.pack("<BBHHH", 12, 1, 12, 4, 5))
assert "rebaseRuiTransformBounds(inst,3,4)" in fixture_source
assert "transformSize[3] = _mm_set_ps(40,40,200,200)" in fixture_source
assert "transformSize[4] = _mm_set_ps(40,40,200,200)" in fixture_source
logic_ids = {n["Id"] for n in fixture["Nodes"] if n["Category"] != "Text Render"}
_, logic_records, logic_applies, logic_source = run(
    "gamestate_info_ffa_logic",
    [n for n in fixture["Nodes"] if n["Id"] in logic_ids],
    [l for l in fixture["Links"] if l["LeftNode"] in logic_ids and l["RightNode"] in logic_ids])
assert logic_records == fixture_records
assert logic_applies == fixture_applies
assert re.search(r" / 60(?:\.0+)?f?;", logic_source)
assert re.search(r"std::fmodf\([^,]+, 60(?:\.0+)?f?\)", logic_source)
assert re.search(r" > 30(?:\.0+)?f?;", logic_source)
assert not re.search(r" / 0(?:\.0+)?f?;", logic_source)
assert "rebaseRuiTransformBounds(inst,4,5)" in fixture_source
print("Transform-group export tests passed.")

names = {1: "Copy Transform", **{i: f"Transform {i}" for i in range(2, 7)},
         7: "2 Pin Scale Transform", 8: "2 Pin Pinch Transform", 9: "2 Pin Stretch Transform",
         10: "3 Pin Transform", 11: "Rotate Transform"}
for type, name in names.items():
    result = run(f"opcode_{type}", [node(1, name)])
    expected = 1 if type == 11 else type
    assert result[0][0] == expected, (type, result[0].hex())
    if type == 11:
        assert result[0][4] == 11 and len(result[0]) == 14
    grouped = run(f"grouped_opcode_{type}", [node(1, name), group(10, [1])])
    assert grouped[0][0] == 0 and grouped[0][1] == expected
print("All registered transform opcode and payload-length tests passed.")
