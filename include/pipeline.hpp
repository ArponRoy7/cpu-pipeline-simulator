#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include "instr.hpp"
#include "metrics.hpp"
#include "hazard.hpp"
#include "predictor.hpp"

// Pipeline register structs
struct IFID  { Instruction ins; bool valid = false; };
struct IDEX  { Instruction ins; bool valid = false; };   // EX stage holds this
struct EXMEM { Instruction ins; bool valid = false; };   // MEM stage holds this
struct MEMWB { Instruction ins; bool valid = false; };   // WB stage holds this

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

    // CSV of pipeline stages (includes WB)
    std::string csv_row() const;

    // Metrics
    const Metrics& metrics() const { return m_; }

private:
    const std::vector<Instruction>& prog_;
    int pc_ = 0;         // next fetch PC
    int cycle_ = 0;
    bool halted_ = false;
    bool forwarding_ = true;

    // Optional predictor (owned by caller)
    BranchPredictor* bp_ = nullptr;

    // pipeline registers
    IFID ifid_;
    IDEX idex_;
    EXMEM exmem_;
    MEMWB memwb_;

    // snapshot of WB stage for csv_row
    Instruction last_wb_ins_{Opcode::NOP};
    bool        last_wb_valid_ = false;

    // control mispredict flush countdown (2 bubbles for EX-resolution)
    int control_flush_bubbles_ = 0;

    // record predicted direction per instruction id (made at ID)
    std::unordered_map<int, bool> pred_taken_by_id_;

    // Metrics
    Metrics m_;
};
