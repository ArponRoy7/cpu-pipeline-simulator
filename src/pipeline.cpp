#include "pipeline.hpp"
#include "trace_loader.hpp"
#include <sstream>

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

    // --- Hazard check for ID against producers in EX/MEM/WB of THIS cycle ---
    HazardDecision hz = detect_hazard_for_ID(
        ifid_.ins,  ifid_.valid,   // ID
        idex_.ins,  idex_.valid,   // EX
        exmem_.ins, exmem_.valid,  // MEM
        memwb_.ins, memwb_.valid,  // WB
        forwarding_
    );

    // --- Branch prediction (record predictions at ID) ---
    // We only RECORD predictions here; we simulate penalties at EX when branch resolves.
    if (bp_ && ifid_.valid &&
        (ifid_.ins.op == Opcode::BEQ || ifid_.ins.op == Opcode::BNE))
    {
        (void)bp_->predict(ifid_.ins.pc); // increments predictor's internal count
        m_.bp_predictions++;
    }

    // --- Advance pipeline (right-to-left), honoring stall & control flush ---
    // WB <= MEM
    MEMWB next_wb = { exmem_.ins, exmem_.valid };
    // MEM <= EX
    EXMEM next_mem = { idex_.ins, idex_.valid };
    // default EX <= ID (may be replaced by stall/flush)
    IDEX next_ex = { ifid_.ins, ifid_.valid };
    // default fetch step (we keep sequential fetch in this step)
    IFID next_ifid = ifid_;

    // If a control flush is active, insert bubbles into EX for N cycles
    if (control_flush_bubbles_ > 0) {
        next_ex = { Instruction{Opcode::NOP}, false };
        control_flush_bubbles_--;
    } else if (hz.stall) {
        // Data hazard stall: bubble EX, hold IF/ID (no fetch advance)
        next_ex = { Instruction{Opcode::NOP}, false };
        m_.stalls.raw++;
    } else {
        // Normal advance: fetch next instruction (sequential)
        if (!halted_ && pc_ < (int)prog_.size()) {
            next_ifid.ins = prog_[pc_++];
            next_ifid.valid = true;
        } else {
            next_ifid.ins = Instruction{Opcode::NOP};
            next_ifid.valid = false;
        }
    }

    // --- Branch resolution at EX: simulate outcome and apply penalty if mispredicted ---
    if (bp_ && idex_.valid &&
        (idex_.ins.op == Opcode::BEQ || idex_.ins.op == Opcode::BNE))
    {
        // Toy "actual" outcome for now: taken if imm < 0 (works for your sample loop)
        bool actual_taken = (idex_.ins.imm < 0);
        // Get what predictor guessed at ID: we re-call predict() to retrieve its current guess.
        // (Our simple predictors update on update(); predict() is idempotent in terms of counts we already did.)
        bool guessed_taken = bp_->predict(idex_.ins.pc);

        // If wrong, simulate a 2-cycle flush penalty (insert 2 bubbles into EX)
        if (guessed_taken != actual_taken) {
            m_.bp_mispredictions++;
            m_.stalls.control += 2;
            control_flush_bubbles_ = 2;
        }

        // Update predictor with the actual outcome
        bp_->update(idex_.ins.pc, actual_taken);
    }

    // Commit stage moves
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
    oss << cycle_ << ","
        << ins_str(ifid_.ins, ifid_.valid) << ","
        << ins_str(idex_.ins, idex_.valid) << ","
        << ins_str(exmem_.ins, exmem_.valid) << ","
        << ins_str(memwb_.ins, memwb_.valid);
    return oss.str();
}
