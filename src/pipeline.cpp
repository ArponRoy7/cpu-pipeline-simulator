#include "pipeline.hpp"
#include "trace_loader.hpp"
#include <sstream>

static inline bool is_branch(const Instruction& ins) {
    return ins.op == Opcode::BEQ || ins.op == Opcode::BNE;
}

// Demo rule for branch outcome (until a real regfile exists):
// "taken" iff imm < 0
static inline bool actual_taken_of(const Instruction& ins) {
    return ins.imm < 0;
}

Pipeline::Pipeline(const std::vector<Instruction>& program,
                   bool forwarding_on,
                   BranchPredictor* bp)
: prog_(program), forwarding_(forwarding_on), bp_(bp) {}

void Pipeline::step() {
    // --- Retire (WB) from previous cycle ---
    if (memwb_.valid) {
        if (memwb_.ins.op == Opcode::HALT) {
            halted_ = true;
        } else if (memwb_.ins.op != Opcode::NOP) {
            m_.retired++;
        }
    }

    // --- Data hazard check for ID vs EX/MEM/WB ---
    HazardDecision hz = detect_hazard_for_ID(
        ifid_.ins,  ifid_.valid,   // ID
        idex_.ins,  idex_.valid,   // EX
        exmem_.ins, exmem_.valid,  // MEM
        memwb_.ins, memwb_.valid,  // WB
        forwarding_
    );

    // ---------- Stage advance skeleton ----------
    // Snapshot the current WB BEFORE shifting; this is what the CSV should show as WB this cycle
    last_wb_ins_   = memwb_.ins;
    last_wb_valid_ = memwb_.valid;

    // WB <= MEM
    MEMWB next_wb  = { exmem_.ins, exmem_.valid };
    // MEM <= EX
    EXMEM next_mem = { idex_.ins,  idex_.valid  };
    // EX default <= ID (may become bubble)
    IDEX  next_ex  = { ifid_.ins,  ifid_.valid  };
    // IF default (may be overwritten by fetch/flush)
    IFID  next_ifid = ifid_;

    // -------- Branch prediction at ID + choose fetch PC --------
    bool can_fetch = true;
    int fetch_pc = pc_; // default sequential

    if (control_flush_bubbles_ > 0) {
        // During control flush: bubble EX and do not fetch
        can_fetch = false;
        next_ex = { Instruction{Opcode::NOP}, false };
        control_flush_bubbles_--;  // consume one bubble this cycle
    } else if (hz.stall) {
        // RAW stall: hold IF/ID, bubble EX, no fetch
        next_ex = { Instruction{Opcode::NOP}, false };
        can_fetch = false;
        m_.stalls.raw++;
    } else {
        // Normal advance: maybe redirect based on ID-stage prediction
        if (bp_ && ifid_.valid && is_branch(ifid_.ins)) {
            const bool pred = bp_->predict(ifid_.ins.pc);
            m_.bp_predictions++;
            pred_taken_by_id_[ifid_.ins.id] = pred;

            const int target = ifid_.ins.pc + 1 + ifid_.ins.imm;
            const int fallth = ifid_.ins.pc + 1;
            fetch_pc = pred ? target : fallth;
        } else {
            fetch_pc = pc_;
        }
    }

    // -------- Fetch into IF/ID (only if allowed) --------
    if (can_fetch) {
        if (!halted_ && fetch_pc >= 0 && fetch_pc < (int)prog_.size()) {
            next_ifid.ins = prog_[fetch_pc];
            next_ifid.valid = true;
            pc_ = fetch_pc + 1; // next default fetch continues after this one
        } else {
            next_ifid.ins = Instruction{Opcode::NOP};
            next_ifid.valid = false;
        }
    }
    // else: keep IF/ID as-is (stall/flush path)

    // -------- Branch resolution at EX --------
    if (idex_.valid && is_branch(idex_.ins) && bp_) {
        const bool actual = actual_taken_of(idex_.ins);

        bool predicted = false;
        if (auto it = pred_taken_by_id_.find(idex_.ins.id); it != pred_taken_by_id_.end())
            predicted = it->second;

        if (predicted != actual) {
            // Mispredict: flush IF & ID (2 bubbles) and redirect PC
            m_.bp_mispredictions++;
            m_.stalls.control += 2;
            control_flush_bubbles_ = 2;

            const int target = idex_.ins.pc + 1 + idex_.ins.imm;
            const int fallth = idex_.ins.pc + 1;
            pc_ = actual ? target : fallth;

            // squash wrong-path next IF
            next_ifid.ins = Instruction{Opcode::NOP};
            next_ifid.valid = false;
        }

        // Train predictor and forget saved pred
        bp_->update(idex_.ins.pc, actual);
        pred_taken_by_id_.erase(idex_.ins.id);
    }

    // -------- Commit stage moves --------
    memwb_ = next_wb;
    exmem_ = next_mem;
    idex_  = next_ex;
    ifid_  = next_ifid;

    // Bookkeeping
    cycle_++;
    m_.cycles++;
}

std::string Pipeline::csv_row() const {
    auto ins_str = [](const Instruction& ins, bool v) {
        if (!v) return std::string("-");
        return opcode_name(ins.op) + "#" + std::to_string(ins.id);
    };
    std::ostringstream oss;
    // 6 columns: cycle, IF, ID, EX, MEM, WB
    oss << cycle_ << ","
        << ins_str(ifid_.ins,  ifid_.valid)  << ","  // IF
        << ins_str(idex_.ins,  idex_.valid)  << ","  // ID
        << ins_str(exmem_.ins, exmem_.valid) << ","  // EX
        << ins_str(memwb_.ins, memwb_.valid) << ","  // MEM
        << ins_str(last_wb_ins_, last_wb_valid_);    // WB (snapshot captured before shift)
    return oss.str();
}
