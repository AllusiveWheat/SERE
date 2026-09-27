"""Run with: python tests/arguments_export.py <SERE.exe> [output-dir].

The engine locates an argument by masking its short hash with argCount-1, so two
names sharing a slot means one of them is unbound in game. A table size cannot
always host a collision free multiplier/addend pair, so the exporter must widen
the table instead of falling back to a degenerate hash.
"""
import json
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

exe = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2] if len(sys.argv) > 2 else "out/group-tests").resolve()
out.mkdir(parents=True, exist_ok=True)

TYPE_NONE = 0x0
TYPE_INT = 0x4
TYPE_FLOAT = 0x5
TYPE_BOOL = 0x3
TYPE_STRING = 0x1

HEADER_ARG_COUNT = 44
HEADER_DATA_STRUCT_SIZE = 34
HEADER_ARG_CLUSTER_OFFSET = 80
HEADER_ARGUMENTS_OFFSET = 88
HEADER_ARG_NAMES_SIZE = 56
HEADER_ARG_NAMES_OFFSET = 72

ARG_TYPES = {"Integer Arg": TYPE_INT, "Float Arg": TYPE_FLOAT,
             "String Arg": TYPE_STRING, "Boolean Arg": TYPE_BOOL}


def arg(id, name, category, **fields):
    return dict(Id=id, Name=name, Category="Argument", PosX=0.0, PosY=0.0, **fields)


def name_at(data, blob, offset):
    end = data.index(b"\0", blob + offset)
    return data[blob + offset:end].decode()


def export(file_name, nodes):
    graph = out / f"{file_name}.json"
    package = out / f"{file_name}.ruip"
    graph.write_text(json.dumps(dict(Nodes=nodes, Links=[])), encoding="utf-8")
    result = subprocess.run([str(exe), "--export-graph", str(graph), str(package),
                             "--size", "1920", "1080"], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, (file_name, result.stdout, result.stderr)

    data = package.read_bytes()
    arg_count = struct.unpack_from("<H", data, HEADER_ARG_COUNT)[0]
    data_struct_size = struct.unpack_from("<H", data, HEADER_DATA_STRUCT_SIZE)[0]
    cluster = struct.unpack_from("<Q", data, HEADER_ARG_CLUSTER_OFFSET)[0]
    arguments = struct.unpack_from("<Q", data, HEADER_ARGUMENTS_OFFSET)[0]
    multiplier = data[cluster + 4]
    addend = data[cluster + 5]

    slots = []
    for index in range(arg_count):
        type, _unk, data_offset, _name_offset, _short_hash = struct.unpack_from("<BBHHH", data, arguments + 8 * index)
        slots.append((type, data_offset))
    return arg_count, data_struct_size, multiplier, addend, slots, data, arguments


def slot_names(data, arguments, arg_count):
    """slot index -> exported argument name, for every occupied slot."""
    blob = struct.unpack_from("<Q", data, HEADER_ARG_NAMES_OFFSET)[0]
    size = struct.unpack_from("<I", data, HEADER_ARG_NAMES_SIZE)[0]
    assert size, "argument names must be exported"
    assert blob and blob + size <= len(data), (blob, size, len(data))
    resolved = {}
    for index in range(arg_count):
        type, _unk, _data_offset, name_offset, _hash = struct.unpack_from("<BBHHH", data, arguments + 8 * index)
        if type == TYPE_NONE:
            continue
        assert name_offset < size, (index, name_offset, size)
        resolved[index] = name_at(data, blob, name_offset)
    return resolved


def counts(slots):
    return Counter(type for type, _ in slots if type != TYPE_NONE)


# A 32 slot table cannot separate these 28 names with any multiplier/addend pair,
# which is the density that used to emit a degenerate hash.
letters = "abcdefghijklmnopqrstuvwxyz"
names = [f"argument{letters[index % 26]}{letters[(index // 26) % 26]}{letters[index % 7]}{index:02d}Row"
         for index in range(28)]
expected = Counter({TYPE_INT: 10, TYPE_FLOAT: 4, TYPE_STRING: 6, TYPE_BOOL: 8})
nodes = []
for index, name in enumerate(names):
    if index < 10:
        nodes.append(arg(100 + index, "Integer Arg", "Argument", ArgName=name, DefaultValue=index))
    elif index < 14:
        nodes.append(arg(100 + index, "Float Arg", "Argument", ArgName=name, DefaultValue=float(index)))
    elif index < 20:
        nodes.append(arg(100 + index, "String Arg", "Argument", ArgName=name, DefaultValue=name))
    else:
        nodes.append(arg(100 + index, "Boolean Arg", "Argument", ArgName=name, DefaultValue=1))

arg_count, data_struct_size, multiplier, addend, slots, data, arguments = export("arguments_many", nodes)
assert counts(slots) == expected, (counts(slots), expected, "collision dropped an argument")
assert multiplier != 0, "degenerate hash would collapse argument names into shared slots"
assert arg_count >= 64 and arg_count & (arg_count - 1) == 0, (arg_count, "table must widen past 32 slots")
offsets = [offset for type, offset in slots if type != TYPE_NONE]
assert len(offsets) == len(set(offsets)), "two arguments share a data offset"
assert all(offset < data_struct_size for offset in offsets), (offsets, data_struct_size)

name_of_slot = slot_names(data, arguments, arg_count)
assert sorted(name_of_slot.values()) == sorted(names), (sorted(name_of_slot.values()), sorted(names))
for node in nodes:
    slot = next(slot for slot, name in name_of_slot.items() if name == node["ArgName"])
    assert slots[slot][0] == ARG_TYPES[node["Name"]], (node["ArgName"], slots[slot][0], node["Name"])

# A small argument set must keep exporting exactly as before.
arg_count, _size, multiplier, addend, slots, data, arguments = export("arguments_few", [
    arg(200, "Integer Arg", "Argument", ArgName="maxTeamScore", DefaultValue=3),
    arg(201, "String Arg", "Argument", ArgName="statusText", DefaultValue="#gamemode_MFD"),
])
assert arg_count >= 2 and arg_count & (arg_count - 1) == 0, arg_count
assert counts(slots) == Counter({TYPE_INT: 1, TYPE_STRING: 1}), counts(slots)
assert sorted(slot_names(data, arguments, arg_count).values()) == ["maxTeamScore", "statusText"]

print("Argument table tests passed.")
