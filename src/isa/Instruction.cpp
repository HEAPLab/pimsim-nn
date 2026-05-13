//
// Created by xyfuture on 2023/6/30.
//
#include "Instruction.h"

#include <algorithm>
#include <array>
#include <stdexcept>
#include <type_traits>

std::map<Opcode,InstType> Instruction::op_to_type = {
        {Opcode::nop,InstType::nop},
        {Opcode::sldi,InstType::scalar},{Opcode::sld,InstType::scalar},{Opcode::sadd,InstType::scalar},
        {Opcode::ssub,InstType::scalar},{Opcode::smul,InstType::scalar},{Opcode::saddi,InstType::scalar},{Opcode::smuli,InstType::scalar},
        {Opcode::setbw,InstType::matrix}, // 特殊设计
        {Opcode::mvmul,InstType::matrix},
        {Opcode::vvadd,InstType::vector},{Opcode::vvsub,InstType::vector},{Opcode::vvmul,InstType::vector},
        {Opcode::vvdmul,InstType::vector},{Opcode::vvmax,InstType::vector},{Opcode::vvsll,InstType::vector},
        {Opcode::vvsra,InstType::vector},{Opcode::vavg,InstType::vector},{Opcode::vrelu,InstType::vector},
        {Opcode::vtanh,InstType::vector},{Opcode::vsigm,InstType::vector},{Opcode::vmv,InstType::vector},
        {Opcode::vrsu,InstType::vector},{Opcode::vrsl,InstType::vector},

        {Opcode::ld,InstType::transfer},{Opcode::st,InstType::transfer},{Opcode::lldi,InstType::transfer},
        {Opcode::lmv,InstType::transfer},{Opcode::send,InstType::transfer},{Opcode::recv,InstType::transfer},
        {Opcode::wait,InstType::transfer},{Opcode::sync,InstType::transfer},
};

namespace {

template <typename T>
T readScalarLE(std::istream& input) {
    std::array<char, sizeof(T)> bytes{};
    input.read(bytes.data(), bytes.size());
    if (!input)
        throw std::runtime_error("Failed to read PIM binary record");
    T value = 0;
    for (size_t index = 0; index < bytes.size(); ++index)
        value |= static_cast<T>(static_cast<unsigned char>(bytes[index])) << (index * 8);
    return value;
}

template <>
int32_t readScalarLE<int32_t>(std::istream& input) {
    return static_cast<int32_t>(readScalarLE<uint32_t>(input));
}

template <typename T>
void writeScalarLE(std::ostream& output, T value) {
    std::array<char, sizeof(T)> bytes{};
    typedef typename std::make_unsigned<T>::type UnsignedT;
    auto raw = static_cast<UnsignedT>(value);
    for (size_t index = 0; index < bytes.size(); ++index)
        bytes[index] = static_cast<char>((raw >> (index * 8)) & 0xff);
    output.write(bytes.data(), bytes.size());
}

Opcode decodeBinaryOpcode(uint32_t opcode) {
    switch (opcode) {
        case 0: return Opcode::nop;
        case 1: return Opcode::sldi;
        case 2: return Opcode::sld;
        case 3: return Opcode::sadd;
        case 4: return Opcode::ssub;
        case 5: return Opcode::smul;
        case 6: return Opcode::saddi;
        case 7: return Opcode::smuli;
        case 8: return Opcode::setbw;
        case 9: return Opcode::mvmul;
        case 10: return Opcode::vvadd;
        case 11: return Opcode::vvsub;
        case 12: return Opcode::vvmul;
        case 13: return Opcode::vvdmul;
        case 14: return Opcode::vvmax;
        case 15: return Opcode::vvsll;
        case 16: return Opcode::vvsra;
        case 17: return Opcode::vavg;
        case 18: return Opcode::vrelu;
        case 19: return Opcode::vtanh;
        case 20: return Opcode::vsigm;
        case 22: return Opcode::vmv;
        case 23: return Opcode::vrsu;
        case 24: return Opcode::vrsl;
        case 25: return Opcode::ld;
        case 26: return Opcode::st;
        case 27: return Opcode::lldi;
        case 28: return Opcode::lmv;
        case 29: return Opcode::send;
        case 30: return Opcode::recv;
        case 31: return Opcode::wait;
        case 32: return Opcode::sync;
        case 21:
            throw std::runtime_error("pimsim-nn does not support binary opcode vsoftmax");
        default:
            throw std::runtime_error("Unsupported PIM binary opcode");
    }
}

uint32_t encodeBinaryOpcode(Opcode opcode) {
    switch (+opcode) {
        case +Opcode::nop: return 0;
        case +Opcode::sldi: return 1;
        case +Opcode::sld: return 2;
        case +Opcode::sadd: return 3;
        case +Opcode::ssub: return 4;
        case +Opcode::smul: return 5;
        case +Opcode::saddi: return 6;
        case +Opcode::smuli: return 7;
        case +Opcode::setbw: return 8;
        case +Opcode::mvmul: return 9;
        case +Opcode::vvadd: return 10;
        case +Opcode::vvsub: return 11;
        case +Opcode::vvmul: return 12;
        case +Opcode::vvdmul: return 13;
        case +Opcode::vvmax: return 14;
        case +Opcode::vvsll: return 15;
        case +Opcode::vvsra: return 16;
        case +Opcode::vavg: return 17;
        case +Opcode::vrelu: return 18;
        case +Opcode::vtanh: return 19;
        case +Opcode::vsigm: return 20;
        case +Opcode::vmv: return 22;
        case +Opcode::vrsu: return 23;
        case +Opcode::vrsl: return 24;
        case +Opcode::ld: return 25;
        case +Opcode::st: return 26;
        case +Opcode::lldi: return 27;
        case +Opcode::lmv: return 28;
        case +Opcode::send: return 29;
        case +Opcode::recv: return 30;
        case +Opcode::wait: return 31;
        case +Opcode::sync: return 32;
        default:
            throw std::runtime_error("Unsupported opcode for binary emission");
    }
}

binary_format::InstructionRecord toBinaryRecord(const Instruction& instruction) {
    binary_format::InstructionRecord record;
    record.opcode = encodeBinaryOpcode(instruction.op);
    record.rd = static_cast<uint8_t>(instruction.rd_addr);
    record.r1 = static_cast<uint8_t>(instruction.rs1_addr);

    switch (instruction.inst_type) {
        case InstType::scalar:
            if (instruction.scalar) {
                record.r2_or_imm = instruction.scalar->imm;
                record.generic2 = instruction.scalar->offset_value;
            }
            break;
        case InstType::matrix:
            if (instruction.op == +Opcode::setbw) {
                record.generic1 = instruction.matrix->ibiw;
                record.generic2 = instruction.matrix->obiw;
            } else if (instruction.matrix) {
                record.r2_or_imm = instruction.matrix->mbiw;
                record.generic1 = instruction.matrix->relu;
                record.generic2 = instruction.matrix->group;
            }
            break;
        case InstType::vector:
            record.r2_or_imm = instruction.rs2_addr;
            if (instruction.vector) {
                record.generic1 = instruction.vector->offset.offset_select;
                record.generic2 = instruction.vector->offset.offset_value;
                record.generic3 = instruction.vector->len;
            }
            break;
        case InstType::transfer:
            if (instruction.transfer) {
                record.generic1 = instruction.transfer->offset.offset_select;
                record.generic2 = instruction.transfer->offset.offset_value;
                record.generic3 = instruction.transfer->len != 0 ? instruction.transfer->len : instruction.transfer->size;
                if (instruction.op == +Opcode::send || instruction.op == +Opcode::recv)
                    record.r2_or_imm = instruction.transfer->core;
                else if (instruction.op == +Opcode::lldi)
                    record.r2_or_imm = instruction.transfer->imm;
            }
            break;
        case InstType::nop:
        case InstType::percesion:
            break;
    }

    return record;
}

Instruction fromBinaryRecord(const binary_format::InstructionRecord& record) {
    Instruction instruction;
    instruction.op = decodeBinaryOpcode(record.opcode);
    instruction.inst_type = Instruction::op_to_type.at(instruction.op);
    instruction.rd_addr = record.rd;
    instruction.rs1_addr = record.r1;

    switch (instruction.inst_type) {
        case InstType::nop:
            break;
        case InstType::scalar:
            instruction.scalar = std::make_shared<ScalarField>();
            instruction.scalar->imm = record.r2_or_imm;
            instruction.scalar->offset_value = record.generic2;
            break;
        case InstType::matrix:
            instruction.matrix = std::make_shared<MatrixField>();
            if (instruction.op == +Opcode::setbw) {
                instruction.matrix->ibiw = record.generic1;
                instruction.matrix->obiw = record.generic2;
            } else {
                instruction.matrix->mbiw = record.r2_or_imm;
                instruction.matrix->relu = record.generic1;
                instruction.matrix->group = record.generic2;
            }
            break;
        case InstType::vector:
            instruction.rs2_addr = record.r2_or_imm;
            instruction.vector = std::make_shared<VectorField>();
            instruction.vector->len = record.generic3;
            instruction.vector->offset.offset_select = record.generic1;
            instruction.vector->offset.offset_value = record.generic2;
            break;
        case InstType::transfer:
            instruction.transfer = std::make_shared<TransferField>();
            instruction.transfer->offset.offset_select = record.generic1;
            instruction.transfer->offset.offset_value = record.generic2;
            if (instruction.op == +Opcode::ld || instruction.op == +Opcode::st
                || instruction.op == +Opcode::send || instruction.op == +Opcode::recv) {
                instruction.transfer->size = record.generic3;
            } else {
                instruction.transfer->len = record.generic3;
            }
            if (instruction.op == +Opcode::send || instruction.op == +Opcode::recv)
                instruction.transfer->core = record.r2_or_imm;
            if (instruction.op == +Opcode::lldi)
                instruction.transfer->imm = record.r2_or_imm;
            break;
        case InstType::percesion:
            break;
    }

    return instruction;
}

} // namespace

std::vector<Instruction> readSingleCoreInstFromBinary(std::istream& binary_stream) {
    char magic[sizeof(binary_format::kMagic)]{};
    binary_stream.read(magic, sizeof(magic));
    if (!binary_stream)
        throw std::runtime_error("Failed to read PIM binary header");
    if (!std::equal(std::begin(magic), std::end(magic), std::begin(binary_format::kMagic)))
        throw std::runtime_error("Invalid PIM binary magic");

    uint32_t version = readScalarLE<uint32_t>(binary_stream);
    if (version != binary_format::kVersion)
        throw std::runtime_error("Unsupported PIM binary version");

    uint32_t instruction_count = readScalarLE<uint32_t>(binary_stream);
    std::vector<Instruction> instructions;
    instructions.reserve(instruction_count);
    for (uint32_t index = 0; index < instruction_count; ++index) {
        binary_format::InstructionRecord record;
        record.opcode = static_cast<uint8_t>(binary_stream.get());
        record.rd = static_cast<uint8_t>(binary_stream.get());
        record.r1 = static_cast<uint8_t>(binary_stream.get());
        record.flags = static_cast<uint8_t>(binary_stream.get());
        if (!binary_stream)
            throw std::runtime_error("Failed to read compact PIM binary header fields");
        record.r2_or_imm = readScalarLE<int32_t>(binary_stream);
        record.generic1 = readScalarLE<int32_t>(binary_stream);
        record.generic2 = readScalarLE<int32_t>(binary_stream);
        record.generic3 = readScalarLE<int32_t>(binary_stream);
        instructions.push_back(fromBinaryRecord(record));
    }
    return instructions;
}

void writeSingleCoreInstToBinary(std::ostream& binary_stream, const std::vector<Instruction>& instructions) {
    binary_stream.write(binary_format::kMagic, sizeof(binary_format::kMagic));
    writeScalarLE<uint32_t>(binary_stream, binary_format::kVersion);
    writeScalarLE<uint32_t>(binary_stream, static_cast<uint32_t>(instructions.size()));
    for (const Instruction& instruction : instructions) {
        auto record = toBinaryRecord(instruction);
        binary_stream.put(static_cast<char>(record.opcode));
        binary_stream.put(static_cast<char>(record.rd));
        binary_stream.put(static_cast<char>(record.r1));
        binary_stream.put(static_cast<char>(record.flags));
        writeScalarLE<int32_t>(binary_stream, record.r2_or_imm);
        writeScalarLE<int32_t>(binary_stream, record.generic1);
        writeScalarLE<int32_t>(binary_stream, record.generic2);
        writeScalarLE<int32_t>(binary_stream, record.generic3);
    }
}
