#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <unordered_set>
#include "trace_loader.hpp"
#include "pipeline.hpp"
#include "predictor_factory.hpp"

static void print_usage(const char* argv0) {
    std::cout <<
        "CPU Pipeline Simulator\n"
        "Usage:\n"
        "  " << argv0 << " --trace <path> [--out <csv>] [--predictor <name>] [--no-forwarding]\n\n"
        "Predictors:\n"
        "  static_nt | static_t | 1bit | 2bit | tournament\n\n";
}

int main(int argc, char** argv) {
    std::string tracePath = "traces/sample.trace";
    std::string outCsv = "data/timeline.csv";
    bool forwarding = true;
    std::string predictor_name = "static_nt";

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--trace" || a == "-t") && i + 1 < argc) { tracePath = argv[++i]; }
        else if (a == "--out" && i + 1 < argc) { outCsv = argv[++i]; }
        else if (a == "--no-forwarding") { forwarding = false; }
        else if (a == "--predictor" && i + 1 < argc) { predictor_name = argv[++i]; }
        else if (a == "--help" || a == "-h") { print_usage(argv[0]); return 0; }
    }

    std::vector<Instruction> prog;
    if (auto err = load_trace(tracePath, prog)) { std::cerr << *err << "\n"; return 1; }
    std::cout << "Loaded " << prog.size() << " instructions\n";

    std::filesystem::path outPath(outCsv);
    if (outPath.has_parent_path()) std::filesystem::create_directories(outPath.parent_path());

    auto predictor = make_predictor(predictor_name);

    Pipeline pipe(prog, forwarding, predictor.get());

    std::ofstream fout(outCsv);
    fout << "cycle,IF,ID,EX,MEM,WB\n";

    const int kMaxCycles = 2000;
    while (!pipe.halted() && pipe.cycle() < kMaxCycles) {
        pipe.step();
        fout << pipe.csv_row() << "\n";
    }

    const Metrics& m = pipe.metrics();
    std::cout << "Done. Cycles=" << m.cycles
              << " Retired=" << m.retired
              << " CPI=" << m.cpi()
              << " StallsRAW=" << m.stalls.raw
              << " StallsCTRL=" << m.stalls.control
              << " TotalStalls=" << m.stalls.total()
              << " Forwarding=" << (forwarding ? "ON" : "OFF")
              << " Predictor=" << predictor->name()
              << " BP_Acc=" << m.bp_accuracy_pct() << "% "
              << "(Pred=" << m.bp_predictions
              << ", Mispred=" << m.bp_mispredictions << ")\n";
    std::cout << "Timeline CSV: " << outCsv << "\n";
    return 0;
}
