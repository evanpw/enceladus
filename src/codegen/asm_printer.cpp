#include "codegen/asm_printer.hpp"
#include "lib/library.h"

void AsmPrinter::printProgram(MachineContext* context)
{
    _out << "bits 64" << std::endl;
    _out << "section .text" << std::endl << std::endl;

    for (const std::string externName : context->externs)
    {
        _out << "extern $" << externName << std::endl;
    }
    _out << std::endl;

    for (MachineFunction* function : context->functions)
    {
        printFunction(function);
    }

    _out << "section .data" << std::endl;

    for (auto& global : context->globals)
    {
        _out << "$" << global.first << ": dq 0" << std::endl;
    }

    for (auto& item : context->staticStrings)
    {
        const std::string& name = item.first;
        const std::string& content = item.second;

        _out << "$" << name << ":" << std::endl;
        _out << "\tdq " << STRING_TAG << ", 0" << std::endl;
        _out << "\tdb \"" << content << "\", 0" << std::endl;
    }

    // Stack map (for the GC)
    _out << "global __stackMap" << std::endl;
    _out << "__stackMap:" << std::endl;
    _out << "\tdq " << _stackMap.size() << std::endl;
    for (size_t i = 0; i < _stackMap.size(); ++i)
    {
        MachineFunction* function = _stackMap[i].function;
        size_t counter = _stackMap[i].counter;
        std::set<int64_t>& variables = _stackMap[i].variables;

        _out << "\t" << "dq "
             << function->name << ".CS" << counter
             << ", " << variables.size();

        for (int64_t offset : variables)
        {
            _out << ", " << offset;
        }

        _out << std::endl;
    }

    // Global variable table (for the GC)
    std::vector<std::string> globalReferences;
    for (auto& global : context->globals)
    {
        if (global.second == ValueType::Reference)
        {
            globalReferences.push_back(global.first);
        }
    }

    _out << "global __globalVarTable" << std::endl;
    _out << "__globalVarTable:" << std::endl;
    _out << "\tdq " << globalReferences.size() << std::endl;
    for (const std::string& globalName : globalReferences)
    {
        _out << "\tdq $" << globalName << std::endl;
    }


    _out << "\tdq 0" << std::endl;

}

void AsmPrinter::printFunction(MachineFunction* function)
{
    _function = function;
    _context = function->context;
    _callSiteCounter = 0;

    _out << "global " << "$" << function->name << std::endl;
    _out << "$" << function->name << ":" << std::endl;

    for (MachineBB* block : function->blocks)
    {
        printBlock(block);
    }

    _out << std::endl;
}

void AsmPrinter::printBlock(MachineBB* block)
{
    _out << "." << block->id << ":" << std::endl;

    for (MachineInst* inst : block->instructions)
    {
        printInstruction(inst);
    }
}

void AsmPrinter::printSimpleOperand(MachineOperand* operand)
{
    if (operand->isVreg())
    {
        HardwareRegister* hreg = getAssignment(operand);
        assert(hreg);

        _out << hreg->name;
    }
    else if (operand->isImmediate())
    {
        _out << *operand;
    }
    else if (operand->isAddress())
    {
        // TODO: Name mangling
        _out << "$" << dynamic_cast<Address*>(operand)->name;
    }
    else
    {
        assert(false);
    }
}

void AsmPrinter::printSimpleInstruction(const std::string& opcode, std::initializer_list<MachineOperand*> operands)
{
    _out << "\t" << opcode;

    bool first = true;
    for (MachineOperand* operand : operands)
    {
        if (first)
        {
            _out << " ";
            printSimpleOperand(operand);
            first = false;
        }
        else
        {
            _out << ", ";
            printSimpleOperand(operand);
        }
    }

    _out << std::endl;
}

void AsmPrinter::printBinary(const std::string& opcode, MachineOperand* dest, MachineOperand* src)
{
    assert(dest->isVreg());

    _out << "\t" << opcode << " ";
    printSimpleOperand(dest);
    _out << ", ";
    printSimpleOperand(src);
    _out << std::endl;
}

void AsmPrinter::printJump(const std::string& opcode, MachineOperand* target)
{
    assert(target->isLabel());
    _out << "\t" << opcode << " ." << dynamic_cast<MachineBB*>(target)->id << std::endl;
}

void AsmPrinter::printCallm(MachineOperand* target)
{
    _out << "\tcall qword [";
    printSimpleOperand(target);
    _out << "]" << std::endl;
}

void AsmPrinter::printMovrm(MachineOperand* dest, MachineOperand* base)
{
    _out << "\tmov ";
    printSimpleOperand(dest);
    _out << ", qword [";

    if (base->isStackLocation())
    {
        StackLocation* stackLocation = dynamic_cast<StackLocation*>(base);
        assert(stackLocation->offset != 0);

        _out << "rbp + " << stackLocation->offset;
    }
    else
    {
        printSimpleOperand(base);
    }
    _out << "]" << std::endl;
}

void AsmPrinter::printMovrm(MachineOperand* dest, MachineOperand* base, MachineOperand* offset)
{
    assert(offset->isImmediate() || offset->isVreg());

    _out << "\tmov ";
    printSimpleOperand(dest);
    _out << ", qword [";
    printSimpleOperand(base);
    _out << " + ";
    printSimpleOperand(offset);
    _out << "]" << std::endl;
}

void AsmPrinter::printMovmd(MachineOperand* base, MachineOperand* src)
{
    if (base->isStackLocation())
    {
        StackLocation* stackLocation = dynamic_cast<StackLocation*>(base);
        assert(stackLocation->offset != 0);

        _out << "\tmov qword [rbp + " << stackLocation->offset << "], ";
        printSimpleOperand(src);
        _out << std::endl;
    }
    else
    {
        _out << "\tmov qword [";
        printSimpleOperand(base);
        _out << "], ";
        printSimpleOperand(src);
        _out << std::endl;
    }
}

void AsmPrinter::printMovmd(MachineOperand* base, MachineOperand* offset, MachineOperand* src)
{
    assert(offset->isImmediate() || offset->isVreg());

    _out << "\tmov qword [";
    printSimpleOperand(base);
    _out << " + ";
    printSimpleOperand(offset);
    _out << "], ";
    printSimpleOperand(src);
    _out << std::endl;
}

void AsmPrinter::printInstruction(MachineInst* inst)
{
    switch (inst->opcode)
    {
        // Simple binary operators
        case Opcode::ADD:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 2);
            assert(inst->outputs[0] == inst->inputs[0]);
            printBinary("add", inst->outputs[0], inst->inputs[1]);
            break;

        case Opcode::AND:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 2);
            assert(inst->outputs[0] == inst->inputs[0]);
            printBinary("and", inst->outputs[0], inst->inputs[1]);
            break;

        case Opcode::SAL:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 2);
            assert(inst->outputs[0] == inst->inputs[0]);
            printBinary("sal", inst->outputs[0], inst->inputs[1]);
            break;

        case Opcode::SAR:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 2);
            assert(inst->outputs[0] == inst->inputs[0]);
            printBinary("sar", inst->outputs[0], inst->inputs[1]);
            break;

        case Opcode::SUB:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 2);
            assert(inst->outputs[0] == inst->inputs[0]);
            printBinary("sub", inst->outputs[0], inst->inputs[1]);
            break;

        case Opcode::IMUL:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 2);
            assert(inst->outputs[0] == inst->inputs[0]);
            printBinary("imul", inst->outputs[0], inst->inputs[1]);
            break;


        // Unary operators
        case Opcode::INC:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 1);
            assert(inst->outputs[0] == inst->inputs[0]);
            assert(inst->outputs[0]->isVreg());
            printSimpleInstruction("inc", {inst->outputs[0]});
            break;


        // Jumps
        case Opcode::JE:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("je", inst->inputs[0]);
            break;

        case Opcode::JG:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("jg", inst->inputs[0]);
            break;

        case Opcode::JGE:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("jge", inst->inputs[0]);
            break;

        case Opcode::JL:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("jl", inst->inputs[0]);
            break;

        case Opcode::JLE:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("jle", inst->inputs[0]);
            break;

        case Opcode::JMP:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("jmp", inst->inputs[0]);
            break;

        case Opcode::JNE:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printJump("jne", inst->inputs[0]);
            break;


        // Memory access
        case Opcode::MOVrm:
            assert(inst->inputs.size() == 1 || inst->inputs.size() == 2);
            assert(inst->outputs.size() == 1);
            assert(inst->outputs[0]->isVreg());
            assert(inst->inputs[0]->isStackLocation() || inst->inputs[0]->isAddress() || inst->inputs[0]->isVreg());
            assert(!inst->inputs[0]->isStackLocation() || inst->inputs.size() == 1);

            if (inst->inputs.size() == 1)
            {
                printMovrm(inst->outputs[0], inst->inputs[0]);
            }
            else
            {
                printMovrm(inst->outputs[0], inst->inputs[0], inst->inputs[1]);
            }

            break;

        case Opcode::MOVmd:
            assert(inst->inputs.size() == 2 || inst->inputs.size() == 3);
            assert(inst->outputs.size() == 0);
            assert(inst->inputs[0]->isStackLocation() || inst->inputs[0]->isAddress() || inst->inputs[0]->isVreg());
            assert(!inst->inputs[0]->isStackLocation() || inst->inputs.size() == 2);

            if (inst->inputs.size() == 2)
            {
                printMovmd(inst->inputs[0], inst->inputs[1]);
            }
            else
            {
                printMovmd(inst->inputs[0], inst->inputs[2], inst->inputs[1]);
            }

            break;


        // Miscellaneous
        case Opcode::MOVrd:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 1);
            printBinary("mov", inst->outputs[0], inst->inputs[0]);
            break;

        case Opcode::CALL:
        {
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() >= 1);
            assert(getAssignment(inst->outputs[0]) == _context->rax);

            // Register arguments
            for (size_t i = 1; i < inst->inputs.size(); ++i)
                assert(inst->inputs[i]->isVreg());

            printSimpleInstruction("call", {inst->inputs[0]});

            // Insert a label for the call site, and record the stack map entry
            // for this call site
            size_t counter = _callSiteCounter++;
            _out << ".CS" << counter << ":" << std::endl;

            std::set<int64_t>& liveVariables = _function->stackMap.at(inst);
            _stackMap.emplace_back(_function, counter, liveVariables);

            break;
        }

        case Opcode::CMP:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 2);
            printSimpleInstruction("cmp", {inst->inputs[0], inst->inputs[1]});
            break;

        case Opcode::TEST:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 2);
            printSimpleInstruction("cmp", {inst->inputs[0], inst->inputs[1]});
            break;

        case Opcode::CQO:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 1);
            assert(getAssignment(inst->outputs[0]) == _context->rdx);
            assert(getAssignment(inst->inputs[0]) == _context->rax);
            printSimpleInstruction("cqo", {});
            break;

        case Opcode::IDIV:
            assert(inst->outputs.size() == 2);
            assert(inst->inputs.size() == 3);
            assert(getAssignment(inst->outputs[0]) == _context->rdx);
            assert(getAssignment(inst->outputs[1]) == _context->rax);
            assert(getAssignment(inst->inputs[0]) == _context->rdx);
            assert(getAssignment(inst->inputs[1]) == _context->rax);
            printSimpleInstruction("idiv", {inst->inputs[2]});
            break;

        case Opcode::DIV:
            assert(inst->outputs.size() == 2);
            assert(inst->inputs.size() == 3);
            assert(getAssignment(inst->outputs[0]) == _context->rdx);
            assert(getAssignment(inst->outputs[1]) == _context->rax);
            assert(getAssignment(inst->inputs[0]) == _context->rdx);
            assert(getAssignment(inst->inputs[1]) == _context->rax);
            printSimpleInstruction("div", {inst->inputs[2]});
            break;

        case Opcode::POP:
            assert(inst->outputs.size() == 1);
            assert(inst->inputs.size() == 0);
            assert(inst->outputs[0]->isVreg());
            printSimpleInstruction("pop", {inst->outputs[0]});
            break;

        case Opcode::PUSH:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() == 1);
            printSimpleInstruction("push", {inst->inputs[0]});
            break;

        case Opcode::RET:
            assert(inst->outputs.size() == 0);
            assert(inst->inputs.size() <= 1);
            assert(inst->inputs.empty() || getAssignment(inst->inputs[0]) == _context->rax);
            printSimpleInstruction("ret", {});
            break;

        default:
            assert(false);
    }
}