#pragma once
#include <cstdint>
#include <span>
#include <set>
#include <stdexcept>
#include <string>

struct TransformBytecodeInfo {
    size_t recordCount = 3;
    std::set<size_t> commandEnds;
};

// Engine wire layouts, verified against engine.dll F4160..F5448.
// Type 0 has no count byte. All other commands have a uint8 count and
// packed uint16 payload fields. Decode explicitly, independently of C++ structs.
inline TransformBytecodeInfo ValidateTransformBytecode(std::span<const uint8_t> data, size_t variableBytes) {
    TransformBytecodeInfo info;
    size_t cursor = 0;
    auto fail = [&](const std::string& reason) {
        throw std::runtime_error("Invalid transform bytecode at byte " + std::to_string(cursor) + ": " + reason);
    };
    auto word = [&](size_t offset) -> uint16_t {
        if (offset + 2 > data.size()) fail("truncated uint16 field");
        return static_cast<uint16_t>(data[offset] | (uint16_t(data[offset + 1]) << 8));
    };
    while (cursor < data.size()) {
        const uint8_t type = data[cursor++];
        if (type == 0) { ++info.recordCount; info.commandEnds.insert(cursor); continue; }
        if (type > 13) fail("unknown opcode " + std::to_string(type));
        if (cursor == data.size()) fail("missing command count");
        const uint8_t count = data[cursor++];
        if (!count) fail("zero command count (engine loops at least once)");
        const size_t stride = type == 1 ? 2 : type <= 6 || type == 13 ? 10 :
            type <= 9 ? 20 : type == 10 ? 30 : type == 11 ? 8 : 6;
        if (size_t(count) * stride > data.size() - cursor) fail("truncated payload for opcode " + std::to_string(type));
        for (unsigned n = 0; n < count; ++n) {
            auto record = [&](size_t field) {
                if (word(cursor + field) >= info.recordCount) fail("transform reference is not allocated");
            };
            auto variable = [&](size_t field) {
                if (size_t(word(cursor + field)) + sizeof(float) > variableBytes) fail("float offset outside data buffer");
            };
            if (type == 12) {
                record(0);
                const auto begin = word(cursor + 2), end = word(cursor + 4);
                if (begin > end || end > info.recordCount) fail("invalid half-open range");
            } else {
                record(0);
                if (type >= 7 && type <= 10) record(6);
                if (type == 10) record(12);
                for (size_t field = 2; field < stride; field += 2) {
                    if (type >= 7 && type <= 10 && field == 6) continue;
                    if (type == 10 && field == 12) continue;
                    variable(field);
                }
                if (type != 11) ++info.recordCount;
            }
            cursor += stride;
        }
        info.commandEnds.insert(cursor);
    }
    return info;
}
