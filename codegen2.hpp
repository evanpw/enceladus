#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "ast_visitor.hpp"

class CodeGen2 : public AstVisitor
{
public:
    virtual void visit(AssignNode* node);
};

#endif
