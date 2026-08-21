//
// Created by xyfuture on 2023/3/1.
//


#pragma once
#ifndef INST_INSTRUCTION_H_
#define INST_INSTRUCTION_H_

#include <sstream>
#include <memory>
#include <map>
#include <fstream>
#include <vector>
#include <cstdint>
#include "nlohmann/json.hpp"
#include "isa/ISA.h"


#define RD_SELECT 1
#define RS1_SELECT 2
#define RS2_SELECT 4


struct OffsetField{
    int offset_value = 0;
    int offset_select = 0;
    // select function

    int getRdOffsetValue() const{
        if (offset_select & RD_SELECT)
            return offset_value;
        return 0;
    }

    int getRs1OffsetValue() const{
        if (offset_value & RS1_SELECT)
            return offset_value;
        return 0;
    }

    int getRs2OffsetValue() const{
        if (offset_value & RS2_SELECT)
            return offset_value;
        return 0;
    }

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(OffsetField,offset_value,offset_select);

    bool operator==(const OffsetField& other) const {
        return offset_value == other.offset_value && offset_select == other.offset_select;
    }
};

struct VectorField{
    int len = 0;
    OffsetField offset ;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(VectorField,len,offset);

    bool operator==(const VectorField& other) const {
        return len == other.len && offset == other.offset;
    }
};

struct MatrixField{
    int mbiw = 0;
    int group = 0;

    int relu = 0;

    int ibiw = 0; // setbw inst use this
    int obiw = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MatrixField,mbiw,group,relu,ibiw,obiw)

    bool operator==(const MatrixField& other) const {
        return mbiw == other.mbiw && group == other.group && relu == other.relu
            && ibiw == other.ibiw && obiw == other.obiw;
    }
};

struct ScalarField{
    int imm = 0;
    int offset_value = 0; // byte

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ScalarField,imm,offset_value);

    bool operator==(const ScalarField& other) const {
        return imm == other.imm && offset_value == other.offset_value;
    }
};

struct TransferField{
    int len = 0 ;
    int size = 0;
    int imm = 0;
    int core = 0; // send/recv/sync
    OffsetField offset ; // byte

    int wait_value = 0;
    int event_register = 0;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TransferField,size,imm,len,core,offset,wait_value,event_register);

    bool operator==(const TransferField& other) const {
        return len == other.len && size == other.size && imm == other.imm && core == other.core
            && offset == other.offset && wait_value == other.wait_value && event_register == other.event_register;
    }

};

namespace binary_format {

static const char kMagic[4] = {'P', 'I', 'M', 'B'};
static const uint32_t kVersion = 1;
static const size_t kHeaderSize = 12;
static const size_t kRecordSize = 20;

struct InstructionRecord {
    uint8_t opcode = 0;
    uint8_t rd = 0;
    uint8_t r1 = 0;
    int32_t r2_or_imm = 0;
    int32_t generic1 = 0;
    int32_t generic2 = 0;
    int32_t generic3 = 0;
    uint8_t flags = 0;
};

} // namespace binary_format

struct Instruction {
    // Common field
    Opcode op = Opcode::nop;
    int rs1_addr = 0;
    int rs2_addr = 0;
    int rd_addr = 0;

    // vector field
    std::shared_ptr<VectorField> vector = nullptr;
    // matrix field
    std::shared_ptr<MatrixField> matrix = nullptr;
    // scalar field
    std::shared_ptr<ScalarField> scalar = nullptr;
    // transfer field
    std::shared_ptr<TransferField> transfer = nullptr;

    InstType inst_type = InstType::nop;

    InstType getInstType() {
        if (inst_type == +InstType::nop) // default value, read again
            inst_type =  (*op_to_type.find(op)).second;
        return inst_type;
    }

    static std::map<Opcode,InstType> op_to_type;


    inline void readInst (const nlohmann::json & inst){
        auto j_op = inst.at("op").get<std::string>();
        op = Opcode::_from_string(j_op.c_str());

        auto type = getInstType();

        if (inst.count("rd") == 1){
            rd_addr = inst["rd"].get<int>();
        }
        if (inst.count("rs1") == 1){
            rs1_addr = inst["rs1"].get<int>();
        }
        if (inst.count("rs2") == 1){
            rs2_addr = inst["rs2"].get<int>();
        }


        switch (type) {
            case InstType::nop:
                break;
            case InstType::scalar:
                scalar = std::make_shared<ScalarField>(inst.get<ScalarField>());
                break;
            case InstType::matrix:
                matrix = std::make_shared<MatrixField>(inst.get<MatrixField>());
                break;
            case InstType::vector:
                vector = std::make_shared<VectorField>(inst.get<VectorField>());
                break;
            case InstType::transfer:
                transfer = std::make_shared<TransferField>(inst.get<TransferField>());
                break;
        }

    }

    Instruction(const nlohmann::json& json_inst){
        readInst(json_inst);
    }

    Instruction() = default;

    bool operator==(const Instruction& other) const {
        return op == other.op && rs1_addr == other.rs1_addr && rs2_addr == other.rs2_addr && rd_addr == other.rd_addr
            && inst_type == other.inst_type
            && ((!vector && !other.vector) || (vector && other.vector && *vector == *other.vector))
            && ((!matrix && !other.matrix) || (matrix && other.matrix && *matrix == *other.matrix))
            && ((!scalar && !other.scalar) || (scalar && other.scalar && *scalar == *other.scalar))
            && ((!transfer && !other.transfer) || (transfer && other.transfer && *transfer == *other.transfer));
    }


    friend std::ostream & operator<<(std::ostream & os, const Instruction& inst) {
        os<<"inst type:"<<inst.inst_type._to_string()<<" opcode:"<<inst.op._to_string();

        if (inst.op == +Opcode::send or inst.op == +Opcode::recv){
            os <<" core_id:"<<inst.transfer->core;
        }

        os<<"\n";
        return os ;
    }


};



inline std::vector<Instruction> readSingleCoreInstFromJson(const nlohmann::json& json_inst_array){
    std::vector<Instruction> tmp;
    for (auto& cur_inst:json_inst_array){
        tmp.emplace_back(cur_inst);
    }
    return tmp;
}

std::vector<Instruction> readSingleCoreInstFromBinary(std::istream& binary_stream);
void writeSingleCoreInstToBinary(std::ostream& binary_stream, const std::vector<Instruction>& instructions);

#endif //INST_INSTRUCTION_H_
