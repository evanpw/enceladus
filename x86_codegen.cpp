#include <algorithm>

#include "x86_codegen.hpp"
#include "ast.hpp"

#define EMIT_BLANK() std::cout << std::endl;
#define EMIT_LEFT(x) std::cout << x << std::endl;
#define EMIT_LABEL(x) std::cout << x << ":" << std::endl;
#define EMIT(x) std::cout << "\t" << x << std::endl;
#define EMIT_COMMENT(x) std::cout << "\t; " << x << std::endl;

void X86CodeGen::generateCode(const TACProgram& program)
{
    //// Program prefix
    EMIT_LEFT("bits 64");
    EMIT_LEFT("section .text");
    EMIT_LEFT("global __main");

    // External references
    if (!program.externs.empty())
    {
        EMIT_LEFT("extern " << program.externs[0]);
        for (size_t i = 1; i < program.externs.size(); ++i)
        {
            EMIT_LEFT("extern " << program.externs[i]);
        }
        EMIT_BLANK();
    }
    EMIT_BLANK();

    // Main function
    generateCode(program.mainFunction);

    // All other functions
    for (auto& function : program.otherFunctions)
    {
        generateCode(function);
    }

    // Declare global variables and string literals in the data segment
    EMIT_BLANK();
    EMIT_LEFT("section .data");
    for (auto& global : program.globals)
    {
        EMIT_LEFT(global->asName().name << ": dq 0");
    }
}

void X86CodeGen::generateCode(const TACFunction& function)
{
    EMIT_COMMENT("begin " << function.name);

    clearRegisters();
    _currentFunction = &function;

    EMIT_LABEL("_" + function.name);
    EMIT("push rbp");
    EMIT("mov rbp, rsp");

    // Assign a location for all of the local vars and temporaries
    _localLocations.clear();
    for (size_t i = 0; i < function.locals.size(); ++i)
    {
        _localLocations[function.locals[i]] = 8 * (i + 1);
    }

    int total = function.locals.size() + function.numberOfTemps;
    if (total > 0) EMIT("add rsp, -" << (8 * total));

    // We have to zero out the local variables for the reference counting
    // to work correctly
    EMIT("mov rax, 0");
    EMIT("mov rcx, " << total);
    EMIT("mov rdi, rsp");
    EMIT("rep stosq");

    for (auto& inst : function.instructions)
    {
        inst->accept(this);
    }

    EMIT_LABEL("__end_" + function.name);
    EMIT("leave");
    EMIT("ret");

    EMIT_COMMENT("end " << function.name);
}

//// Helper functions //////////////////////////////////////////////////////////

// TODO: Push/pop register state
void X86CodeGen::clearRegisters()
{
    _registers.clear();

    for (std::string regName : {"rax", "rbx", "rcx", "rdx", "rdi", "rsi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"})
    {
        _registers[regName] = {true, false, false, nullptr};
    }
}

std::string X86CodeGen::access(std::shared_ptr<Address> address, bool forRead)
{
    std::string reg;
    if (getRegisterContaining(address, reg))
    {
        if (!forRead) _registers[reg].isDirty = true;
        return reg;
    }
    else
    {
        return accessDirectly(address);
    }
}

std::string X86CodeGen::accessDirectly(std::shared_ptr<Address> address)
{
    if (address->tag == AddressTag::Name)
    {
        NameAddress& nameAddress = address->asName();
        std::stringstream result;

        if (nameAddress.nameTag == NameTag::Global)
        {
            result << "[rel " << nameAddress.name << "]";
        }
        else if (nameAddress.nameTag == NameTag::Local)
        {
            auto i = _localLocations.find(address);
            assert(i != _localLocations.end());

            result << "[rbp - " << i->second << "]";
        }
        else if (nameAddress.nameTag == NameTag::Param)
        {
            const std::vector<std::shared_ptr<Address>>& params = _currentFunction->params;

            auto i = std::find(params.begin(), params.end(), address);
            assert (i != params.end());

            size_t index = i - params.begin();
            result << "[rbp + " << 8 * (2 + index) << "]";
        }
        else if (nameAddress.nameTag == NameTag::Function)
        {
            return nameAddress.name;
        }
        else
        {
            assert(false);
        }

        return result.str();
    }
    else if (address->tag == AddressTag::Temp)
    {
        TempAddress& tempAddress = address->asTemp();

        int offset = (_localLocations.size() + 1 + tempAddress.number) * 8;

        std::stringstream result;
        result << "[rbp - " << offset << "]";
        return result.str();
    }
    else
    {
        return address->str();
    }
}

bool X86CodeGen::getRegisterContaining(std::shared_ptr<Address> address, std::string& reg)
{
    for (auto& i : _registers)
    {
        if (i.second.value == address)
        {
            reg = i.first;
            return true;
        }
    }

    return false;
}

bool X86CodeGen::getEmptyRegister(std::string& emptyRegister)
{
    for (auto& i : _registers)
    {
        if (i.second.isFree)
        {
            emptyRegister = i.first;
            return true;
        }
    }

    return false;
}

std::string X86CodeGen::spillRegister()
{
    // Cannot spill registers which are in use (in the current instruction).

    // Prefer to "spill" registers which aren't dirty, since they don't need to
    // be written out to memory
    for (auto& i : _registers)
    {
        if (!i.second.inUse && !i.second.isDirty)
        {
            return i.first;
        }
    }

    for (auto& i : _registers)
    {
        if (!i.second.inUse)
        {
            EMIT_COMMENT("Spill " << i.second.value->str());
            EMIT("mov " << accessDirectly(i.second.value) << ", " << i.first);

            i.second.isFree = true;
            i.second.value.reset();
            i.second.isDirty = false;
            i.second.inUse = false;

            return i.first;
        }
    }

    // All registers are in use? Shouldn't ever happen
    assert(false);
}

void X86CodeGen::spillAndClear()
{
    for (auto& i : _registers)
    {
        if (i.second.isDirty)
        {
            assert(!i.second.isFree);

            EMIT_COMMENT("Spill " << i.second.value->str());
            EMIT("mov " << accessDirectly(i.second.value) << ", " << i.first);

            i.second.isFree = true;
            i.second.value.reset();
            i.second.isDirty = false;
            i.second.inUse = false;
        }
    }

    clearRegisters();
}

std::string X86CodeGen::getRegisterFor(std::shared_ptr<Address> address, bool forRead)
{
    std::string reg;

    // Priority: already containing address, empty, spillable
    if (!getRegisterContaining(address, reg))
    {
        if (!getEmptyRegister(reg))
        {
            reg = spillRegister();
        }

        if (forRead)
        {
            EMIT("mov " << reg << ", " << accessDirectly(address));
            _registers[reg].isDirty = false;
        }
    }

    if (!forRead)
        _registers[reg].isDirty = true;

    _registers[reg].isFree = false;
    _registers[reg].inUse = true;
    _registers[reg].value = address;

    return reg;
}

std::string X86CodeGen::getScratchRegister()
{
    std::string reg;
    if (!getEmptyRegister(reg))
    {
        reg = spillRegister();
    }

    _registers[reg].isFree = false;
    _registers[reg].inUse = true;
    _registers[reg].value = nullptr;
    _registers[reg].isDirty = false;

    return reg;
}

// TODO: If already in a register, exchange
std::string X86CodeGen::getSpecificRegisterFor(std::shared_ptr<Address> address, std::string reg, bool forRead)
{
    assert(!_registers[reg].inUse);

    if (_registers[reg].value != address)
    {
        if (_registers[reg].isDirty)
        {
            assert(!_registers[reg].isFree);

            EMIT_COMMENT("Spill " << _registers[reg].value->str());
            EMIT("mov " << accessDirectly(_registers[reg].value) << ", " << reg);
        }

        if (forRead)
        {
            EMIT("mov " << reg << ", " << access(address));
            _registers[reg].isDirty = false;
        }
    }

    if (!forRead)
        _registers[reg].isDirty = true;

    _registers[reg].isFree = false;
    _registers[reg].inUse = true;
    _registers[reg].value = address;

    return reg;
}

void X86CodeGen::evictRegister(std::string reg)
{
    assert(!_registers[reg].inUse);

    if (_registers[reg].isFree) return;

    if (_registers[reg].isDirty)
    {
        EMIT_COMMENT("Spill " << _registers[reg].value->str());
        EMIT("mov " << accessDirectly(_registers[reg].value) << ", " << reg);
    }

    _registers[reg].isFree = true;
    _registers[reg].value.reset();
    _registers[reg].isDirty = false;
    _registers[reg].inUse = true;
}

void X86CodeGen::freeRegister(std::string reg)
{
    _registers[reg].inUse = false;
}

//// Individual TAC Instruction Handlers ///////////////////////////////////////

void X86CodeGen::codeGen(const TACConditionalJump* inst)
{
    EMIT_COMMENT(inst->str());

    std::string lhs = getRegisterFor(inst->lhs, true);
    std::string rhs = getRegisterFor(inst->rhs, true);

    spillAndClear();

    EMIT("cmp " << lhs << ", " << rhs);

    if (inst->op == ">")
    {
        EMIT("jg " << inst->target->str());
    }
    else if (inst->op == "<")
    {
        EMIT("jl " << inst->target->str());
    }
    else if (inst->op == "==")
    {
        EMIT("je " << inst->target->str());
    }
    else if (inst->op == "!=")
    {
        EMIT("jne " << inst->target->str());
    }
    else if (inst->op == ">=")
    {
        EMIT("jge " << inst->target->str());
    }
    else if (inst->op == "<=")
    {
        EMIT("jle " << inst->target->str());
    }
    else
    {
        assert(false);
    }
}

void X86CodeGen::codeGen(const TACJumpIf* inst)
{
    EMIT_COMMENT(inst->str());

    std::string lhs = getRegisterFor(inst->lhs, true);

    spillAndClear();

    EMIT("cmp " << lhs << ", 11b");
    EMIT("je " << inst->target->str());
}

void X86CodeGen::codeGen(const TACJumpIfNot* inst)
{
    EMIT_COMMENT(inst->str());

    std::string lhs = getRegisterFor(inst->lhs, true);

    spillAndClear();

    EMIT("cmp " << lhs << ", 11b");
    EMIT("jne " << inst->target->str());
}

void X86CodeGen::codeGen(const TACAssign* inst)
{
    EMIT_COMMENT(inst->str());

    std::string rhs = getRegisterFor(inst->rhs, true);
    EMIT("mov " << access(inst->lhs, false) << ", " << rhs);

    freeRegister(rhs);
}

void X86CodeGen::codeGen(const TACJump* inst)
{
    EMIT_COMMENT(inst->str());

    spillAndClear();
    EMIT("jmp " << inst->target->str());
}

void X86CodeGen::codeGen(const TACLabel* inst)
{
    EMIT_COMMENT(inst->str());

    spillAndClear();
    EMIT_LABEL(inst->label->str());
}

void X86CodeGen::codeGen(const TACCall* inst)
{
    EMIT_COMMENT(inst->str());

    if (inst->foreign)
    {
        // x86_64 calling convention for C puts the first 6 arguments in registers
        std::string registerArgs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

        assert(inst->params.size() <= 6);

        for (size_t i = 0; i < inst->params.size(); ++i)
        {
            getSpecificRegisterFor(inst->params[i], registerArgs[i], true);
        }

        std::string stackSave = getScratchRegister();

        // Realign the stack to 16 bytes (may not be necessary on all platforms)
        EMIT("mov " << stackSave << ", rsp");
        EMIT("and rsp, -16");
        EMIT("add rsp, -8");
        EMIT("push " << stackSave);

        spillAndClear();

        EMIT("call " << inst->function);

        if (inst->dest)
        {
            getSpecificRegisterFor(inst->dest, "rax", false);
            freeRegister("rax");
        }

        stackSave = getScratchRegister();

        // Undo the stack alignment
        EMIT("pop " << stackSave);
        EMIT("mov rsp, " << stackSave);

        freeRegister(stackSave);
    }
    else
    {
        for (auto i = inst->params.rbegin(); i != inst->params.rend(); ++i)
        {
            std::shared_ptr<Address> param = *i;
            EMIT("push qword " << access(*i));
        }

        spillAndClear();

        EMIT("call " << inst->function);

        if (inst->dest)
        {
            getSpecificRegisterFor(inst->dest, "rax", false);
            freeRegister("rax");
        }
    }
}

void X86CodeGen::codeGen(const TACIndirectCall* inst)
{
    EMIT_COMMENT(inst->str());

    for (auto& param : inst->params)
    {
        EMIT("push qword " << access(param));
    }

    spillAndClear();

    EMIT("call " << access(inst->function));

    getSpecificRegisterFor(inst->dest, "rax", false);
    freeRegister("rax");
}

void X86CodeGen::codeGen(const TACRightIndexedAssignment* inst)
{
    EMIT_COMMENT(inst->str());

    std::string lhs = getRegisterFor(inst->lhs, false);
    std::string rhs = getRegisterFor(inst->rhs, true);

    EMIT("mov " << lhs <<  ", [" << rhs << " + " << inst->offset << "]");

    freeRegister(lhs);
    freeRegister(rhs);
}

void X86CodeGen::codeGen(const TACLeftIndexedAssignment* inst)
{
    EMIT_COMMENT(inst->str());

    std::string lhs = getRegisterFor(inst->lhs, true);
    std::string rhs = getRegisterFor(inst->rhs, true);

    EMIT("mov [" << lhs << " + " << inst->offset << "], " << rhs);

    freeRegister(lhs);
    freeRegister(rhs);
}

void X86CodeGen::codeGen(const TACBinaryOperation* inst)
{
    EMIT_COMMENT(inst->str());

    std::string lhs, rhs, dest;


    if (inst->op == "+" || inst->op == "-" || inst->op == "*")
    {
        lhs = getRegisterFor(inst->lhs, true);
        rhs = getRegisterFor(inst->rhs, true);
        dest = getRegisterFor(inst->dest, false);

        if (inst->op == "+")
        {
            EMIT("mov " << dest << ", " << lhs);
            EMIT("xor " << dest << ", " << 1);      // Clear tag bit
            EMIT("add " << dest << ", " << rhs);
        }
        else if (inst->op == "-")
        {
            dest = getRegisterFor(inst->dest, false);

            EMIT("mov " << dest << ", " << lhs);
            EMIT("sub " << dest << ", " << rhs);
            EMIT("inc " << dest);                   // Restore tag bit
        }
        else /* if (inst->op == "*") */
        {
            dest = getRegisterFor(inst->dest, false);

            // Need to do this first, in case lhs == rhs
            EMIT("mov " << dest << ", " << lhs);

            freeRegister(rhs);
            evictRegister(rhs);
            EMIT("sar " << rhs << ", 1");                           // Shift out tag bit

            EMIT("sar " << dest << ", 1");
            EMIT("imul " << dest << ", " << rhs);
            EMIT("lea " << dest << ", [2 * " << dest << " + 1]");   // Re-insert tag bit
        }
    }
    else if (inst->op == "/")
    {
        dest = getSpecificRegisterFor(inst->dest, "rax", false);
        evictRegister("rdx");
        lhs = getRegisterFor(inst->lhs, true);
        rhs = getRegisterFor(inst->rhs, true);

        EMIT("mov rax, " << lhs);

        freeRegister(rhs);
        evictRegister(rhs);
        EMIT("sar " << rhs << ", 1");       // Shift out tag bit

        EMIT("sar rax, 1");
        EMIT("cqo");
        EMIT("idiv " << rhs);
        EMIT("lea rax, [2 * rax + 1]");     // Re-insert tag bit

        freeRegister("rdx");
    }
    else if (inst->op == "%")
    {
        dest = getSpecificRegisterFor(inst->dest, "rdx", false);
        evictRegister("rax");
        lhs = getRegisterFor(inst->lhs, true);
        rhs = getRegisterFor(inst->rhs, true);

        EMIT("mov rax, " << lhs);

        freeRegister(rhs);
        evictRegister(rhs);
        EMIT("sar " << rhs << ", 1");       // Shift out tag bit

        EMIT("sar rax, 1");
        EMIT("cqo");
        EMIT("idiv " << rhs);
        EMIT("lea rdx, [2 * rdx + 1]");     // Re-insert tag bit

        freeRegister("rax");
    }

    freeRegister(lhs);
    freeRegister(rhs);
    freeRegister(dest);
}

void X86CodeGen::codeGen(const TACReturn* inst)
{
    EMIT_COMMENT(inst->str());

    std::string result = getSpecificRegisterFor(inst->result, "rax", true);
    EMIT("jmp " << "__end_" + _currentFunction->name);
    freeRegister(result);
}

