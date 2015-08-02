#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include <map>
#include <vector>
#include "ast.hpp"

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

    // Declarations
    virtual void visit(DataDeclaration* node);
    virtual void visit(ForeignDeclNode* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(LetNode* node);
    virtual void visit(StructDefNode* node);
    virtual void visit(TypeAliasNode* node);

    // Internal nodes
    virtual void visit(AssignNode* node);
    virtual void visit(BlockNode* node);
    virtual void visit(ComparisonNode* node);
    virtual void visit(ConstructorSpec* node);
    virtual void visit(ForeachNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(IfNode* node);
    virtual void visit(LogicalNode* node);
    virtual void visit(MatchArm* node);
    virtual void visit(MatchNode* node);
    virtual void visit(ProgramNode* node);
    virtual void visit(SwitchNode* node);
    virtual void visit(WhileNode* node);

    // Leaf nodes
    virtual void visit(BoolNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(MemberDefNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(VariableNode* node);

private:
    //// Type Inference ////////////////////////////////////////////////////////
    Type* newVariable();

    static void inferenceError(AstNode* node, const std::string& msg);

    static bool occurs(TypeVariable* variable, Type* value);
    void unify(Type* lhs, Type* rhs, AstNode* node);
    void bindVariable(Type* variable, Type* value, AstNode* node);

    static TypeScheme* generalize(Type* type, const std::vector<Scope*>& scopes);
    Type* instantiate(Type* type, const std::map<TypeVariable*, Type*>& replacements);
    Type* instantiate(TypeScheme* scheme);

    static std::set<TypeVariable*> getFreeVars(Symbol& symbol);

    //// General semantic analysis /////////////////////////////////////////////
    template<typename... Args>
    void semanticError(const YYLTYPE& location, const std::string& str, Args... args);

    FunctionSymbol* makeBuiltin(const std::string& name);
    FunctionSymbol* makeExternal(const std::string& name);
    void injectSymbols();

    TypeConstructor* getTypeConstructor(const TypeName* typeName);
    TypeConstructor* getTypeConstructor(const YYLTYPE& location, const std::string& name);
    void resolveBaseType(TypeName* typeName, std::unordered_map<std::string, Type*>& variables, bool createVariables=false);
    void resolveTypeName(TypeName* typeName, bool createVariables=false);
    void resolveTypeName(TypeName* typeName, std::unordered_map<std::string, Type*>& variables, bool createVariables=false);

    void insertSymbol(Symbol* symbol);
    void releaseSymbol(Symbol* symbol);

    Scope* topScope() { return _scopes.back(); }
    Symbol* resolveSymbol(const std::string& name);
    Symbol* resolveTypeSymbol(const std::string& name);
    void enterScope(Scope* scope) { _scopes.push_back(scope); }
    void exitScope() { _scopes.pop_back(); }

    ProgramNode* _root;
    AstContext* _context;
    TypeTable* _typeTable;
    FunctionDefNode* _enclosingFunction;
    LoopNode* _enclosingLoop;
    std::vector<Scope*> _scopes;
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
