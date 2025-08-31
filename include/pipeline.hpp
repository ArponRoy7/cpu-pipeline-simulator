#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "instr.hpp"
#include "metrics.hpp"
#include "hazard.hpp"
#include "predictor.hpp"

// Pipeline register structs (classic 5-stage: IF, ID, EX, MEM, WB)
struct IFID  { Instruction ins; bool valid = false; };
struct IDEX  { Instruction ins; bool valid = false; };   // ID stage register feeding EX
struct EXMEM { Instruction ins; bool valid = false; };   // EX stage register feeding MEM
struct MEMWB { Instruction ins; bool valid = false; };   // MEM stage register feeding WB

class Pipeline {
public:
    Pipeline(const std::vector<Instruction>& program,
             bool forwarding_on = true,
             BranchPredictor* bp = nullptr);

    // Advance one cycle
    void step();

    // State
    bool halted() const { return halted_; }
    int  cycle()  const { return cycle_; }

    // CSV of pipeline stages (6 columns): cycle,IF,ID,EX,MEM,WB
    std::string csv_row() const;

    // Metrics
    const Metrics& metrics() const { return m_; }

private:
    // Helpers
    static inline bool is_branch(const Instruction& ins) {
        return ins.op == Opcode::BEQ || ins.op == Opcode::BNE;
    }
    // Toy ground-truth: branch taken iff imm < 0 (consistent with prior samples)
    static inline bool actual_taken_of(const Instruction& ins) {
        return ins.imm < 0;
    }

private:
    const std::vector<Instruction>& prog_;
    int  pc_       = 0;     // next fetch PC
    int  cycle_    = 0;
    bool halted_   = false;
    bool forwarding_ = true;

    // Optional predictor (not owned)
    BranchPredictor* bp_ = nullptr;

    // Pipeline registers (latched at end of cycle)
    IFID  ifid_;
    IDEX  idex_;
    EXMEM exmem_;
    MEMWB memwb_;

    // WB snapshot from previous cycle (so CSV shows the instruction that just retired)
    Instruction last_wb_ins_{Opcode::NOP};
    bool        last_wb_valid_ = false;

    // Control mispredict flush countdown (2 bubbles for EX-resolution)
    int control_flush_bubbles_ = 0;

    // Bookkeep predicted direction per-instruction (prediction made in ID)
    std::unordered_map<int, bool> pred_taken_by_id_;

    // Label for the bubble we explicitly inserted this cycle into the ID→EX slot.
    // Example values: "", "STALL_RAW", "STALL_CTRL", (future: "STALL_WAR", "STALL_WAW")
    std::string ex_bubble_label_;

    // Metrics
    Metrics m_;
};
