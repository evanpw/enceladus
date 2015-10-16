#ifndef MACHINE_CONTEXT_HPP
#define MACHINE_CONTEXT_HPP

#include "codegen/machine_instruction.hpp"

#include <memory>
#include <unordered_map>
#include <vector>

class MachineContext
{
public:
    ~MachineContext();

    std::vector<MachineFunction*> functions;
    std::vector<std::string> externs;
    std::vector<std::pair<std::string, std::string>> staticStrings;
    std::vector<std::pair<std::string, ValueType>> globals;

    HardwareRegister* rax = new HardwareRegister("rax", "eax", "ax", "al");
    HardwareRegister* rbx = new HardwareRegister("rbx", "ebx", "bx", "bl");
    HardwareRegister* rcx = new HardwareRegister("rcx", "ecx", "cx", "cl");
    HardwareRegister* rdx = new HardwareRegister("rdx", "edx", "dx", "dl");
    HardwareRegister* rsi = new HardwareRegister("rsi", "esi", "si", "");
    HardwareRegister* rdi = new HardwareRegister("rdi", "edi", "di", "");
    HardwareRegister* rbp = new HardwareRegister("rbp", "ebp", "bp", "");
    HardwareRegister* rsp = new HardwareRegister("rsp", "esp", "sp", "");
    HardwareRegister* r8 = new HardwareRegister("r8", "r8d", "r8w", "r8b");
    HardwareRegister* r9 = new HardwareRegister("r9", "r9d", "r9w", "r9b");
    HardwareRegister* r10 = new HardwareRegister("r10", "r10d", "r10w", "r10b");
    HardwareRegister* r11 = new HardwareRegister("r11", "r11d", "r11w", "r11b");
    HardwareRegister* r12 = new HardwareRegister("r12", "r12d", "r12w", "r12b");
    HardwareRegister* r13 = new HardwareRegister("r13", "r13d", "r13w", "r13b");
    HardwareRegister* r14 = new HardwareRegister("r14", "r14d", "r14w", "r14b");
    HardwareRegister* r15 = new HardwareRegister("r15", "r15d", "r15w", "r15b");

    HardwareRegister* hregs[16] = {
        rax, rbx, rcx, rdx, rsi, rdi, r8, r9,
        r10, r11, r12, r13, r14, r15, rbp, rsp
    };

    Immediate* createImmediate(int64_t value, ValueType type);
    Address* createGlobal(const std::string& name, bool clinkage = false);

private:
    std::vector<std::unique_ptr<Immediate>> _immediates;
    std::unordered_map<std::string, std::unique_ptr<Address>> _globals;
};

#endif
