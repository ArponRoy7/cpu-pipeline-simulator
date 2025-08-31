#pragma once
#include <vector>
#include <string>
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
    int pc_ = 0;         // program counter (index into prog_)
    int cycle_ = 0;
    bool halted_ = false;
    bool forwarding_ = true;

    // Optional branch predictor (owned by caller)
    BranchPredictor* bp_ = nullptr;

    // pipeline registers
    IFID ifid_;
    IDEX idex_;
    EXMEM exmem_;
    MEMWB memwb_;

    // control mispredict flush countdown (simulate 2-cycle flush)
    int control_flush_bubbles_ = 0;

    // metrics
    Metrics m_;
};
