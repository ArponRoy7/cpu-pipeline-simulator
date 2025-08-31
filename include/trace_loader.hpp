#pragma once
#include <string>
#include <vector>
#include <optional>
#include "instr.hpp"

// Loads a text trace and returns parsed instructions, or error string.
std::optional<std::string> load_trace(
    const std::string& path,
    std::vector<Instruction>& out);

// Utility to pretty print an instruction (defined in .cpp)
std::string opcode_name(Opcode op);
