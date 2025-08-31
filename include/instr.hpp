#pragma once
#include <string>
#include <cstdint>

enum class Opcode {
    ADD,    // ADD  rd rs1 rs2
    SUB,    // SUB  rd rs1 rs2
    LOAD,   // LOAD rd [rs1+imm]
    STORE,  // STORE rs2 [rs1+imm]
    BEQ,    // BEQ  rs1 rs2 imm   (PC-relative offset, in instructions)
    BNE,    // BNE  rs1 rs2 imm
    NOP,    // NOP
    HALT    // HALT
};

// Toy ISA register file size (can change later)
constexpr int kNumRegs = 32;

// Unified instruction container for the pipeline
struct Instruction {
    Opcode op{};
    int    rd   = -1;   // dest register (if any)
    int    rs1  = -1;   // source 1 (if any)
    int    rs2  = -1;   // source 2 (if any)
    int    imm  = 0;    // immediate (offset for LOAD/STORE, branch displacement)
    int    id   = -1;   // globally unique instruction id (for timeline)
    int    pc   = -1;   // index in the trace (0-based)

    // Human-readable (for debugging & CSV)
    std::string to_string() const;
};
