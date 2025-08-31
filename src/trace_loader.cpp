#include "trace_loader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::toupper(c); });
    return s;
}

static bool parse_reg(const std::string& tok, int& reg_out) {
    // Accept: r0, R0, x0, X0, or plain number
    if (tok.empty()) return false;
    std::string t = tok;
    if (t[0] == 'r' || t[0] == 'R' || t[0] == 'x' || t[0] == 'X') {
        t = t.substr(1);
    }
    try {
        int v = std::stoi(t);
        if (v < 0 || v >= kNumRegs) return false;
        reg_out = v;
        return true;
    } catch (...) {
        return false;
    }
}

std::string opcode_name(Opcode op) {
    switch (op) {
        case Opcode::ADD:   return "ADD";
        case Opcode::SUB:   return "SUB";
        case Opcode::LOAD:  return "LOAD";
        case Opcode::STORE: return "STORE";
        case Opcode::BEQ:   return "BEQ";
        case Opcode::BNE:   return "BNE";
        case Opcode::NOP:   return "NOP";
        case Opcode::HALT:  return "HALT";
    }
    return "UNK";
}

std::string Instruction::to_string() const {
    std::ostringstream oss;
    oss << "#" << id << " PC=" << pc << " " << opcode_name(op);
    switch (op) {
        case Opcode::ADD:
        case Opcode::SUB:
            oss << " r" << rd << " r" << rs1 << " r" << rs2; break;
        case Opcode::LOAD:
            oss << " r" << rd << " [r" << rs1 << (imm>=0?"+":"") << imm << "]"; break;
        case Opcode::STORE:
            oss << " r" << rs2 << " [r" << rs1 << (imm>=0?"+":"") << imm << "]"; break;
        case Opcode::BEQ:
        case Opcode::BNE:
            oss << " r" << rs1 << " r" << rs2 << " " << imm; break;
        case Opcode::NOP:
        case Opcode::HALT:
            break;
    }
    return oss.str();
}

static bool parse_mem_operand(const std::string& tok, int& baseReg, int& imm) {
    // format: [rX+imm] or [rX-imm] or [rX]
    if (tok.size() < 3 || tok.front() != '[' || tok.back() != ']') return false;
    std::string inner = tok.substr(1, tok.size()-2);
    // find + or - if present
    size_t plus = inner.find('+');
    size_t minus = inner.find('-');
    if (plus == std::string::npos && minus == std::string::npos) {
        // just [rX]
        if (!parse_reg(inner, baseReg)) return false;
        imm = 0; return true;
    }
    size_t sep = (plus != std::string::npos ? plus : minus);
    std::string lhs = inner.substr(0, sep);
    std::string rhs = inner.substr(sep);
    if (!parse_reg(lhs, baseReg)) return false;
    try { imm = std::stoi(rhs); } catch (...) { return false; }
    return true;
}

std::optional<std::string> load_trace(
    const std::string& path,
    std::vector<Instruction>& out)
{
    std::ifstream in(path);
    if (!in) return std::string("Could not open trace: ") + path;

    out.clear();
    std::string line;
    int pc = 0;
    int nextId = 0;

    while (std::getline(in, line)) {
        // strip comments beginning with '#'
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string opTok;
        iss >> opTok;
        opTok = upper(opTok);

        Instruction ins;
        ins.id = nextId++;
        ins.pc = pc++;

        if (opTok == "ADD" || opTok == "SUB") {
            std::string rd, rs1, rs2;
            if (!(iss >> rd >> rs1 >> rs2)) return "Bad ADD/SUB at line: " + line;
            if (!parse_reg(rd, ins.rd) || !parse_reg(rs1, ins.rs1) || !parse_reg(rs2, ins.rs2))
                return "Bad register in ADD/SUB at line: " + line;
            ins.op = (opTok == "ADD") ? Opcode::ADD : Opcode::SUB;
        } else if (opTok == "LOAD") {
            std::string rd, mem;
            if (!(iss >> rd >> mem)) return "Bad LOAD at line: " + line;
            if (!parse_reg(rd, ins.rd)) return "Bad dest reg in LOAD at line: " + line;
            if (!parse_mem_operand(mem, ins.rs1, ins.imm)) return "Bad mem operand in LOAD at line: " + line;
            ins.op = Opcode::LOAD;
        } else if (opTok == "STORE") {
            std::string rs2, mem;
            if (!(iss >> rs2 >> mem)) return "Bad STORE at line: " + line;
            if (!parse_reg(rs2, ins.rs2)) return "Bad src reg in STORE at line: " + line;
            if (!parse_mem_operand(mem, ins.rs1, ins.imm)) return "Bad mem operand in STORE at line: " + line;
            ins.op = Opcode::STORE;
        } else if (opTok == "BEQ" || opTok == "BNE") {
            std::string rs1, rs2, immTok;
            if (!(iss >> rs1 >> rs2 >> immTok)) return "Bad BEQ/BNE at line: " + line;
            if (!parse_reg(rs1, ins.rs1) || !parse_reg(rs2, ins.rs2))
                return "Bad reg in BEQ/BNE at line: " + line;
            try { ins.imm = std::stoi(immTok); } catch (...) { return "Bad imm in BEQ/BNE at line: " + line; }
            ins.op = (opTok == "BEQ") ? Opcode::BEQ : Opcode::BNE;
        } else if (opTok == "NOP") {
            ins.op = Opcode::NOP;
        } else if (opTok == "HALT") {
            ins.op = Opcode::HALT;
        } else {
            return "Unknown opcode: " + opTok;
        }

        out.push_back(ins);
    }
    return std::nullopt; // success
}
