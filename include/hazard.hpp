#pragma once
#include "instr.hpp"

// Decision for the ID stage this cycle.
enum class HazardKind { None, RAW, WAR, WAW };

struct HazardDecision {
    bool stall = false;        // if true, hold IF/ID and insert a bubble into ID/EX
    HazardKind kind = HazardKind::None;
};

// Compute hazards for the instruction currently in ID against producers ahead.
// forwarding_on = true : allow EX/MEM and MEM/WB bypasses (but still handle load-use)
// forwarding_on = false: require data to be available only after WB
HazardDecision detect_hazard_for_ID(const Instruction& id_ins, bool id_valid,
                                    const Instruction& ex_ins, bool ex_valid,
                                    const Instruction& mem_ins, bool mem_valid,
                                    const Instruction& wb_ins,  bool wb_valid,
                                    bool forwarding_on);
