#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"

#include <map>
#include <vector>

class SemanticError : public std::exception
{
public:
    SemanticError(const std::string& description)
    : _description(description)
    {}

    virtual ~SemanticError() throw() {}

    const std::string& description() const { return _description; }
    virtual const char* what() const throw() { return _description.c_str(); }


private:
    std::string _description;
};

class SemanticAnalyzer : public AstVisitor
{
public:
    SemanticAnalyzer(AstContext* context);
    bool analyze();

    // Internal nodes
    virtual void visit(AssertNode* node);
    virtual void visit(AssignNode* node);
    virtual void visit(BinopNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(CastNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(ConstructorSpec* node);
    virtual void visit(DataDeclaration* node);
    virtual void visit(ForeachNode* node);
    virtual void visit(ForeignDeclNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(ImplNode* node);
    virtual void visit(LetNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchArm* node);
    virtual void visit(MatchNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(MethodCallNode* node);
    virtual void visit(MethodDefNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(StructDefNode* node);
    virtual void visit(TraitDefNode* node);
    virtual void visit(TypeAliasNode* node);
    virtual void visit(VariableDefNode* node);
    virtual void visit(WhileNode* node);

    // Leaf nodes
    virtual void visit(BoolNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(MemberDefNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(PassNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(TraitMethodNode* node);

private:
    //// General semantic analysis /////////////////////////////////////////////
    FunctionSymbol* createBuiltin(const std::string& name);
    FunctionSymbol* createExternal(const std::string& name);
    void injectSymbols();

    using TypeContext = std::unordered_map<std::string, Type*>;
    std::vector<TypeContext> _typeContexts;
    Type* findInContext(const std::string& varName);

    Type* getConstructedType(const TypeName* typeName);
    Type* getConstructedType(const YYLTYPE& location, const std::string& name);
    void resolveBaseType(TypeName* typeName);
    void resolveTypeName(TypeName* typeName);

    Symbol* resolveSymbol(const std::string& name);
    Symbol* resolveTypeSymbol(const std::string& name);
    void resolveMemberSymbol(const std::string& name, Type* parentType, std::vector<MemberSymbol*>& symbols);

    TraitSymbol* resolveTrait(TypeName* traitName, std::vector<Type*>& traitParams);
    void addConstraints(TypeVariable* var, const std::vector<TypeName*>& constraints);
    void resolveTypeParams(AstNode* node, const std::vector<TypeParam>& typeParams, std::unordered_map<std::string, Type*>& typeContext);
    void resolveTypeParams(AstNode* node, const std::vector<TypeParam>& typeParams, std::unordered_map<std::string, Type*>& typeContext, std::vector<Type*>& variables);

    ProgramNode* _root;
    AstContext* _context;
    TypeTable* _typeTable;
    SymbolTable* _symbolTable;
    FunctionDefNode* _enclosingFunction;
    LoopNode* _enclosingLoop;
    ImplNode* _enclosingImplNode;
    TraitDefNode* _enclosingTraitDef;
};

class SemanticPass2 : public AstVisitor
{
public:
    SemanticPass2(AstContext* context);
    bool analyze();

    virtual void visit(BinopNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(IntNode* node);

private:
    AstContext* _context;
    TypeTable* _typeTable;
};

class TypeInferenceError : public std::exception
{
public:
    TypeInferenceError(const std::string& description)
    : _description(description)
    {}

    virtual ~TypeInferenceError() throw() {}

    const std::string& description() const { return _description; }
    virtual const char* what() const throw() { return _description.c_str(); }


private:
    std::string _description;
};

#endif
