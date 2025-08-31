#include "hazard.hpp"

// Helpers
static inline bool writes_reg(const Instruction& ins) {
    switch (ins.op) {
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::LOAD:
            return ins.rd >= 0;
        default:
            return false;
    }
}
static inline int dest_reg(const Instruction& ins) {
    return (writes_reg(ins) ? ins.rd : -1);
}
static inline bool reads_r1(const Instruction& ins) {
    switch (ins.op) {
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::LOAD:  // base address
        case Opcode::STORE:
        case Opcode::BEQ:
        case Opcode::BNE:
            return ins.rs1 >= 0;
        default: return false;
    }
}
static inline bool reads_r2(const Instruction& ins) {
    switch (ins.op) {
        case Opcode::ADD:
        case Opcode::SUB:
        case Opcode::STORE:
        case Opcode::BEQ:
        case Opcode::BNE:
            return ins.rs2 >= 0;
        default: return false;
    }
}

HazardDecision detect_hazard_for_ID(const Instruction& id_ins, bool id_valid,
                                    const Instruction& ex_ins, bool ex_valid,
                                    const Instruction& mem_ins, bool mem_valid,
                                    const Instruction& wb_ins,  bool wb_valid,
                                    bool forwarding_on)
{
    HazardDecision d;

    if (!id_valid) return d; // no instruction in ID

    const bool id_reads1 = reads_r1(id_ins);
    const bool id_reads2 = reads_r2(id_ins);
    const int r1 = id_ins.rs1;
    const int r2 = id_ins.rs2;

    // --- RAW hazards only (for in-order 5-stage, WAR/WAW don't actually require stalls) ---
    // With forwarding ON:
    //   * if EX is a LOAD producing rd and ID uses that rd -> load-use hazard: ONE stall.
    //   * otherwise, EX/MEM & MEM/WB producers can be forwarded -> no stall.
    //
    // With forwarding OFF:
    //   * any producer in EX/MEM/WB whose rd matches ID.rs1 or ID.rs2 -> stall.

    auto check_raw_match = [&](const Instruction& prod, bool valid) -> bool {
        if (!valid) return false;
        int rd = dest_reg(prod);
        if (rd < 0) return false;
        return (id_reads1 && r1 == rd) || (id_reads2 && r2 == rd);
    };

    if (forwarding_on) {
        // load-use from EX stage
        if (ex_valid && ex_ins.op == Opcode::LOAD && check_raw_match(ex_ins, true)) {
            d.stall = true;
            d.kind = HazardKind::RAW;
            return d;
        }
        // Everything else is forwardable -> no stall.
        return d;
    } else {
        // No forwarding: any pending producer causes a stall
        if (check_raw_match(ex_ins, ex_valid) ||
            check_raw_match(mem_ins, mem_valid) ||
            check_raw_match(wb_ins,  wb_valid)) {
            d.stall = true;
            d.kind = HazardKind::RAW;
            return d;
        }
        return d;
    }
}
