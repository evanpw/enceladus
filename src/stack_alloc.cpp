#include "stack_alloc.hpp"
#include "machine_context.hpp"

StackAlloc::StackAlloc(MachineFunction* function)
: _function(function), _context(_function->context)
{
}

void StackAlloc::run()
{
    // TODO: Compute interference graph and use the same offsets for some non-interfering locations
    int64_t _nextOffset = -8;
    for (size_t i = 0; i < _function->stackVariableCount(); ++i)
    {
        StackLocation* stackVar = _function->getStackVariable(i);
        stackVar->offset = _nextOffset;

        _nextOffset -= 8;
    }

    int64_t neededRoom = -(_nextOffset + 8);

    // In the entry block, allocate room for the stack variables
    if (neededRoom != 0)
    {
        // Round up to mantain 16-byte alignment
        if (neededRoom % 16)
            neededRoom += 8;

        MachineBB* entryBlock = *(_function->blocks.begin());

        auto itr = entryBlock->instructions.begin();

        // HACK: First two instructions will always be push rbp; mov rbp, rsp
        ++itr;
        ++itr;

        MachineInst* allocInst = new MachineInst(Opcode::ADD, {_context->rsp}, {_context->rsp, _context->makeImmediate(-neededRoom)});
        entryBlock->instructions.insert(itr, allocInst);
    }
}