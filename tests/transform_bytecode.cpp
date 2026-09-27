#include "RuiNodeEditor/TransformBytecode.h"
#include <vector>
#include <iostream>

static void invalid(std::vector<uint8_t> bytes, size_t variables = 4) {
    try { ValidateTransformBytecode(bytes, variables); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error("Malformed bytecode was accepted");
}

int main() {
    // Early null, two copies, then a type-12 application of slot 3 to [4,6).
    const std::vector<uint8_t> valid {0, 1,2, 2,0, 2,0, 12,1, 3,0, 4,0, 6,0};
    const auto result = ValidateTransformBytecode(valid, 4);
    if (result.recordCount != 6 || result.commandEnds != std::set<size_t>{1,7,15})
        throw std::runtime_error("Incorrect command boundaries or record allocation");
    invalid({14});                         // Unknown type.
    invalid({2});                          // Missing count.
    invalid({1,0});                        // Engine's do/while count underflow.
    invalid({10,1,2,0});                   // Truncated 30-byte payload.
    invalid({1,1,3,0});                    // Forward reference.
    invalid({11,1,2,0,4,0,0,0,0,0});     // Rotation value beyond variable buffer.
    invalid({12,1,2,0,3,0,2,0});          // Reversed half-open range.
    invalid({12,1,2,0,0,0,4,0});          // Range includes unallocated record.
    std::vector<uint8_t> threePin(32);
    threePin[0] = 10; threePin[1] = 1;
    if (ValidateTransformBytecode(threePin, 4).recordCount != 4) throw std::runtime_error("Type 10 allocation");
    threePin[0] = 7;                       // Original wrong-opcode regression.
    const auto wrong = ValidateTransformBytecode(threePin, 4);
    if (wrong.recordCount == 4) throw std::runtime_error("Wrong opcode escaped allocation-count check");
    std::cout << "Transform bytecode validation tests passed.\n";
}
