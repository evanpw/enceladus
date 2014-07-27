#ifndef X86_CODEGEN_HPP
#define X86_CODEGEN_HPP

#include "address.hpp"
#include "tac_instruction.hpp"
#include "tac_program.hpp"
#include "tac_visitor.hpp"

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

class X86CodeGen : public TACVisitor
{
public:
    virtual void generateCode(TACProgram& program);

    void visit(TACConditionalJump* inst);
    void visit(TACJumpIf* inst);
    void visit(TACJumpIfNot* inst);
    void visit(TACAssign* inst);
    void visit(TACJump* inst);
    void visit(TACLabel* inst);
    void visit(TACCall* inst);
    void visit(TACIndirectCall* inst);
    void visit(TACRightIndexedAssignment* inst);
    void visit(TACLeftIndexedAssignment* inst);
    void visit(TACBinaryOperation* inst);

private:
    void generateCode(TACFunction& function);

    bool inRegister(std::shared_ptr<Address> address);
    bool inMemory(std::shared_ptr<Address> address);
    bool isConst(std::shared_ptr<Address> address);

    static const int READ = 1;
    static const int WRITE = 2;

    std::string access(std::shared_ptr<Address> address, int readWrite = READ);
    std::string accessDirectly(std::shared_ptr<Address> address);

    // Get a register to be used only inside of the code for a single TAC
    // instruction. It's assumed that after the register is not currently in
    // use, it has no meaningful value
    std::string getScratchRegister();

    // Find a register for a given address. If forRead, then load it in.
    std::string getRegisterFor(std::shared_ptr<Address> address, int readWrite = READ);

    // Load the given address into a specific register, evicting the previous
    // address if necessary. (This is for operations like division that use a
    // specific register).
    std::string getSpecificRegisterFor(std::shared_ptr<Address> address, std::string reg, int readWrite = READ);

    // Similar to getSpecificRegisterFor, but just does the assignment without
    // any moving. The register must be free to succeed. This function is used
    // to map function parameters stored in registers to locations on the stack
    // where they can be spilled.
    std::string assignRegister(std::shared_ptr<Address> address, std::string reg);

    // Evict the current value of the given register, if any, and write back to
    // memory if necessary. This is used for registers which may be overwritten
    // by some operation, like rdx in division
    void evictRegister(std::string reg);

    // Find a register which is not currently in use, write it back out to
    // memory if necessary, and return it.
    std::string spillRegister();

    // Spill all dirty registers and reset everything we know about what is
    // stored in each register. We don't do anything clever with the
    // control-flow graph, so we have to do this before any jump and before any
    // label
    void spillAndClear();

    // Find a register which contains no value. On success, return true and set
    // the output parameter
    bool getEmptyRegister(std::string& emptyRegister);

    // Find a register which contains the given value. On success, return true
    // and set the output parameter
    bool getRegisterContaining(std::shared_ptr<Address> address, std::string& reg);

    // Mark the given register as no longer in use (in the current instruction)
    // so that it can be spilled or reused as necessary
    void freeRegister(std::string reg);

    void clearRegisters();

    struct RegisterDescriptor
    {
        bool isFree;
        bool inUse;
        bool isDirty;

        std::shared_ptr<Address> value;
    };

    std::unordered_map<std::string, RegisterDescriptor> _registers;

    const TACFunction* _currentFunction = nullptr;
    int _numberOfLocals = 0;
    std::unordered_map<std::shared_ptr<Address>, size_t> _localLocations;
};

#endif
