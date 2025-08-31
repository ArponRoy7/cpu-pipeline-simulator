#pragma once
#include <string>
#include <unordered_map>

// Branch predictor base class
class BranchPredictor {
public:
    virtual ~BranchPredictor() = default;

    // Predict whether branch at PC is taken
    virtual bool predict(int pc) = 0;

    // Update predictor state with actual outcome
    virtual void update(int pc, bool taken) = 0;

    // Human-readable name
    virtual std::string name() const = 0;

    // Stats
    int total_predictions = 0;
    int mispredictions    = 0;

    double accuracy() const {
        return total_predictions > 0
            ? 100.0 * (total_predictions - mispredictions) / total_predictions
            : 0.0;
    }
};

// --------------------------- Implementations ---------------------------

// Static always-taken / always-not-taken
class StaticPredictor : public BranchPredictor {
public:
    explicit StaticPredictor(bool taken) : always_taken_(taken) {}
    bool predict(int) override { total_predictions++; return always_taken_; }
    void update(int, bool actual) override {
        if (always_taken_ != actual) mispredictions++;
    }
    std::string name() const override {
        return always_taken_ ? "Static-AlwaysTaken" : "Static-AlwaysNotTaken";
    }
private:
    bool always_taken_;
};

// 1-bit predictor (per-PC)
class OneBitPredictor : public BranchPredictor {
public:
    bool predict(int pc) override {
        total_predictions++;
        auto it = table.find(pc);
        if (it == table.end()) return false; // default not taken
        return it->second;
    }
    void update(int pc, bool actual) override {
        bool pred = predict(pc);
        if (pred != actual) mispredictions++;
        table[pc] = actual;
    }
    std::string name() const override { return "OneBit"; }
private:
    std::unordered_map<int, bool> table; // pc -> last outcome
};

// 2-bit saturating counter predictor
class TwoBitPredictor : public BranchPredictor {
public:
    bool predict(int pc) override {
        total_predictions++;
        int state = table[pc];
        return state >= 2; // 2 or 3 = predict taken
    }
    void update(int pc, bool actual) override {
        bool pred = predict(pc);
        if (pred != actual) mispredictions++;
        int &state = table[pc];
        // saturating counter: 0..3
        if (actual) {
            if (state < 3) state++;
        } else {
            if (state > 0) state--;
        }
    }
    std::string name() const override { return "TwoBit"; }
private:
    std::unordered_map<int, int> table; // pc -> state (0..3)
};
