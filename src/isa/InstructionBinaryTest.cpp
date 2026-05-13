#include "isa/Instruction.h"

#include <cassert>
#include <sstream>

int main() {
    const auto json = nlohmann::json::parse(R"json(
[
  {"imm":64,"op":"sldi","rd":0},
  {"imm":128,"op":"sldi","rd":1},
  {"len":16,"offset":{"offset_select":0,"offset_value":0},"op":"lmv","rd":0,"rs1":1},
  {"group":3,"mbiw":8,"op":"mvmul","rd":0,"relu":0,"rs1":1},
  {"len":16,"offset":{"offset_select":0,"offset_value":0},"op":"vvadd","rd":0,"rs1":1,"rs2":2},
  {"core":2,"offset":{"offset_select":0,"offset_value":0},"op":"send","rd":0,"size":16}
]
)json");

    const std::vector<Instruction> json_instructions = readSingleCoreInstFromJson(json);

    std::stringstream binary_stream(std::ios::in | std::ios::out | std::ios::binary);
    writeSingleCoreInstToBinary(binary_stream, json_instructions);
    binary_stream.seekg(0);

    const std::vector<Instruction> binary_instructions = readSingleCoreInstFromBinary(binary_stream);

    assert(json_instructions == binary_instructions);
    return 0;
}
