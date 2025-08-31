#pragma once
#include <cstdint>

struct StallBreakdown {
    uint64_t raw = 0;       // Read-After-Write
    uint64_t war = 0;       // Write-After-Read (kept for completeness)
    uint64_t waw = 0;       // Write-After-Write (kept for completeness)
    uint64_t control = 0;   // branch-related flush bubbles
    uint64_t total() const { return raw + war + waw + control; }
};

struct Metrics {
    uint64_t cycles = 0;
    uint64_t retired = 0;        // committed (non-NOP, non-HALT) instructions

    // Branch prediction
    uint64_t bp_predictions = 0;
    uint64_t bp_mispredictions = 0;

    StallBreakdown stalls;

    double cpi() const { return retired ? double(cycles) / double(retired) : 0.0; }
    double bp_accuracy_pct() const {
        return bp_predictions ? 100.0 * (double(bp_predictions - bp_mispredictions) / double(bp_predictions)) : 0.0;
    }
};
