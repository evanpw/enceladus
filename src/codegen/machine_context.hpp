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
    std::vector<std::string> globals;

    HardwareRegister* rax = new HardwareRegister("rax");
    HardwareRegister* rbx = new HardwareRegister("rbx");
    HardwareRegister* rcx = new HardwareRegister("rcx");
    HardwareRegister* rdx = new HardwareRegister("rdx");
    HardwareRegister* rsi = new HardwareRegister("rsi");
    HardwareRegister* rdi = new HardwareRegister("rdi");
    HardwareRegister* rbp = new HardwareRegister("rbp");
    HardwareRegister* rsp = new HardwareRegister("rsp");
    HardwareRegister* r8 = new HardwareRegister("r8");
    HardwareRegister* r9 = new HardwareRegister("r9");
    HardwareRegister* r10 = new HardwareRegister("r10");
    HardwareRegister* r11 = new HardwareRegister("r11");
    HardwareRegister* r12 = new HardwareRegister("r12");
    HardwareRegister* r13 = new HardwareRegister("r13");
    HardwareRegister* r14 = new HardwareRegister("r14");
    HardwareRegister* r15 = new HardwareRegister("r15");

    HardwareRegister* hregs[16] = {
        rax, rbx, rcx, rdx, rsi, rdi, r8, r9,
        r10, r11, r12, r13, r14, r15, rbp, rsp
    };

    Immediate* makeImmediate(int64_t value);
    Address* makeGlobal(const std::string& name);

private:
    std::unordered_map<int64_t, std::unique_ptr<Immediate>> _immediates;
    std::unordered_map<std::string, std::unique_ptr<Address>> _globals;
};

#endif
