#include "pipeline.hpp"
#include "trace_loader.hpp"
#include <sstream>

Pipeline::Pipeline(const std::vector<Instruction>& program,
                   bool forwarding_on,
                   BranchPredictor* bp)
: prog_(program), forwarding_(forwarding_on), bp_(bp) {}

void Pipeline::step() {
    // --- Retire (WB) from previous cycle: snapshot MEM/WB so CSV shows WB this cycle ---
    last_wb_ins_   = memwb_.ins;
    last_wb_valid_ = memwb_.valid;

    if (memwb_.valid) {
        if (memwb_.ins.op == Opcode::HALT) {
            halted_ = true;
        } else if (memwb_.ins.op != Opcode::NOP) {
            m_.retired++;
        }
    }

    // --- Data hazard check for the instruction currently in ID stage (ifid_) ---
    HazardDecision hz = detect_hazard_for_ID(
        ifid_.ins,  ifid_.valid,   // ID
        idex_.ins,  idex_.valid,   // EX
        exmem_.ins, exmem_.valid,  // MEM
        memwb_.ins, memwb_.valid,  // WB
        forwarding_
    );

    // ---------- Compute next pipeline registers (WB <- MEM <- EX <- ID) ----------
    MEMWB next_wb  = { exmem_.ins, exmem_.valid }; // WB gets previous EX/MEM
    EXMEM next_ex  = { idex_.ins,  idex_.valid  }; // EX gets previous ID/EX (shown as EX in CSV)
    IDEX  next_id  = { ifid_.ins,  ifid_.valid  }; // ID gets previous IF/ID
    IFID  next_if  =  ifid_;                       // IF/ID defaults to hold; fetch may overwrite

    // -------- Decide fetch behaviour & potential ID bubble insertion --------
    bool can_fetch = true;
    int  fetch_pc  = pc_; // default is to continue from pc_

    if (control_flush_bubbles_ > 0) {
        // Control hazard flush: insert a bubble into the ID→EX slot
        next_id = { Instruction{Opcode::NOP}, false };
        ex_bubble_label_ = "STALL_CTRL";
        can_fetch = false;               // kill fetch this cycle
        control_flush_bubbles_--;
        m_.stalls.control++;             // count bubble cycles individually
    } else if (hz.stall) {
        // Data hazard stall: bubble ID→EX and hold IF/ID; do not fetch
        next_id = { Instruction{Opcode::NOP}, false };
        ex_bubble_label_ = "STALL_RAW";
        can_fetch = false;
        m_.stalls.raw++;
    } else {
        ex_bubble_label_.clear();        // normal advance; no bubble from ID
        // Perform branch prediction at ID to choose next fetch PC
        if (bp_ && ifid_.valid && is_branch(ifid_.ins)) {
            bool pred = bp_->predict(ifid_.ins.pc);
            m_.bp_predictions++;
            pred_taken_by_id_[ifid_.ins.id] = pred;
            int target  = ifid_.ins.pc + 1 + ifid_.ins.imm;
            int fall_th = ifid_.ins.pc + 1;
            fetch_pc = pred ? target : fall_th;
        } else {
            fetch_pc = pc_;
        }
    }

    // -------- Fetch into IF/ID (only if allowed) --------
    if (can_fetch) {
        if (!halted_ && fetch_pc >= 0 && fetch_pc < (int)prog_.size()) {
            next_if.ins = prog_[fetch_pc];
            next_if.valid = true;
            pc_ = fetch_pc + 1; // default next sequential
        } else {
            next_if.ins = Instruction{Opcode::NOP};
            next_if.valid = false;
        }
    } // else: hold IF/ID and do not change pc_

    // -------- Branch resolution at EX (the instruction that was in ID last cycle) --------
    if (idex_.valid && is_branch(idex_.ins) && bp_) {
        bool actual = actual_taken_of(idex_.ins);
        bool predicted = false;
        if (auto it = pred_taken_by_id_.find(idex_.ins.id); it != pred_taken_by_id_.end()) {
            predicted = it->second;
        }

        if (predicted != actual) {
            // Mispredict: redirect and flush IF & ID in the *next* two cycles (bubble count)
            m_.bp_mispredictions++;
            control_flush_bubbles_ = 2;

            int target  = idex_.ins.pc + 1 + idex_.ins.imm;
            int fall_th = idex_.ins.pc + 1;
            pc_ = actual ? target : fall_th;

            // Squash any wrong-path fetch we may have placed for the upcoming cycle
            next_if.ins = Instruction{Opcode::NOP};
            next_if.valid = false;
        }

        // Train predictor with ground truth
        bp_->update(idex_.ins.pc, actual);
        pred_taken_by_id_.erase(idex_.ins.id);
    }

    // -------- Commit new stage registers --------
    memwb_ = next_wb;
    exmem_ = next_ex;
    idex_  = next_id;
    ifid_  = next_if;

    // Bookkeeping
    cycle_++;
    m_.cycles++;
}

std::string Pipeline::csv_row() const {
    auto ins_str = [](const Instruction& ins, bool v) {
        if (!v) return std::string("-");
        return opcode_name(ins.op) + "#" + std::to_string(ins.id);
    };

    // We show the bubble label in the ID column (the slot we bubbled)
    auto id_cell = [&]() -> std::string {
        if (!idex_.valid && !ex_bubble_label_.empty()) return ex_bubble_label_;
        return ins_str(idex_.ins, idex_.valid);
    };

    std::ostringstream oss;
    // 6 columns: cycle,IF,ID,EX,MEM,WB
    oss << cycle_ << ","
        << ins_str(ifid_.ins,  ifid_.valid)   << ","
        << id_cell()                          << ","
        << ins_str(exmem_.ins, exmem_.valid)  << ","
        << ins_str(memwb_.ins, memwb_.valid)  << ","
        << ins_str(last_wb_ins_, last_wb_valid_);
    return oss.str();
}
