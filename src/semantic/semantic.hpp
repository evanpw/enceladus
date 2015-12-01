#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP

#include "ast/ast.hpp"
#include "semantic/return_checker.hpp"
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
    virtual void visit(EnumDeclaration* node);
    virtual void visit(ForeignDeclNode* node);
    virtual void visit(ForeverNode* node);
    virtual void visit(ForNode* node);
    virtual void visit(FunctionCallNode* node);
    virtual void visit(FunctionDefNode* node);
    virtual void visit(IfElseNode* node);
    virtual void visit(ImplNode* node);
    virtual void visit(IndexNode* node);
    virtual void visit(LambdaNode* node);
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
    virtual void visit(AssociatedTypeNode* node);
    virtual void visit(BoolNode* node);
    virtual void visit(BreakNode* node);
    virtual void visit(ContinueNode* node);
    virtual void visit(IntNode* node);
    virtual void visit(StructVarNode* node);
    virtual void visit(NullaryNode* node);
    virtual void visit(PassNode* node);
    virtual void visit(ReturnNode* node);
    virtual void visit(StringLiteralNode* node);
    virtual void visit(TraitMethodNode* node);

private:
    friend class LValueAnalyzer;

    //// General semantic analysis /////////////////////////////////////////////
    FunctionSymbol* createBuiltin(const std::string& name);
    FunctionSymbol* createExternal(const std::string& name);
    void injectSymbols();

    using TypeContext = std::unordered_map<std::string, Type*>;
    std::vector<TypeContext> _typeContexts;
    std::vector<std::unordered_set<Type*>> _inferredVars; // Contains a subset of _typeContexts consisting only of inferred type variables
    Type* findInContext(const std::string& varName);
    void pushTypeContext(TypeContext&& typeContext = {});
    void popTypeContext();

    Type* getConstructedType(const TypeName* typeName);
    Type* getConstructedType(const YYLTYPE& location, const std::string& name);
    void resolveBaseType(TypeName* typeName, bool inferVariables = false);
    void resolveTypeName(TypeName* typeName, bool inferVariables = false);

    Symbol* resolveSymbol(const std::string& name);
    Symbol* resolveTypeSymbol(const std::string& name);
    void resolveMemberSymbol(const std::string& name, Type* parentType, std::vector<MemberSymbol*>& symbols);

    TraitSymbol* resolveTrait(TypeName* traitName, std::vector<Type*>& traitParams, bool inferVariables = false);
    std::pair<bool, std::string> addConstraints(Type* lhs, const std::vector<TypeName*>& constraints, bool inferVariables = false);
    void resolveTypeParam(AstNode* node, const TypeParam& typeParams, std::vector<Type*>& variables);
    void resolveTypeParams(AstNode* node, const std::vector<TypeParam>& typeParams, std::vector<Type*>& variables);
    void resolveWhereClause(AstNode* node, const std::vector<TypeParam>& typeParams);
    void resolveTypeNameWhere(AstNode* node, TypeName* typeName, const std::vector<TypeParam>& whereClause);

    void checkTraitCoherence();

    ProgramNode* _root;
    AstContext* _context;
    TypeTable* _typeTable;
    SymbolTable* _symbolTable;

    FunctionDefNode* _enclosingFunction = nullptr;
    LoopNode* _enclosingLoop = nullptr;
    ImplNode* _enclosingImplNode = nullptr;
    TraitDefNode* _enclosingTraitDef = nullptr;
    LambdaNode* _enclosingLambda = nullptr;

    /// Deferred processing
    bool _allowDefer = true;

    struct DeferredNode
    {
        DeferredNode(AstNode* node, const SymbolTable::SavedScopes& scopes)
        : node(node), scopes(scopes)
        {}

        AstNode* node;
        SymbolTable::SavedScopes scopes;
    };

    std::vector<DeferredNode> _deferred;

    void defer(AstNode* node)
    {
        node->type = _typeTable->createTypeVariable();
        _deferred.emplace_back(node, _symbolTable->saveScopes());
    }

    bool restore(const DeferredNode& deferredNode)
    {
        _symbolTable->restoreScopes(deferredNode.scopes);
        deferredNode.node->accept(this);

        bool found = false;
        for (auto& item : _deferred)
        {
            if (item.node == deferredNode.node)
            {
                return false;
            }
        }

        return true;
    }

    ReturnChecker _returnChecker;
};

// WARNING: Both the main analyzer and the lvalue analyzer can run on the same
// node (e.g., in += statements), so they need to be compatible. In other
// words, if both analyzers write to the same annotation (e.g., the type),
// they must give it the same value
class LValueAnalyzer : public AstVisitor
{
public:
    LValueAnalyzer(SemanticAnalyzer* mainAnalyzer)
    : _mainAnalyzer(mainAnalyzer)
    {}

    virtual void visit(NullaryNode* node);
    virtual void visit(MemberAccessNode* node);
    virtual void visit(IndexNode* node);

    bool good() const { return _good; }

private:
    bool _good = false;
    SemanticAnalyzer* _mainAnalyzer;
};

class EnvironmentCapture : public AstVisitor
{
public:
    EnvironmentCapture(SymbolTable* symbolTable, VariableSymbol* envSymbol, std::unordered_set<Symbol*>&& boundVars)
    : _symbolTable(symbolTable), _envSymbol(envSymbol), _boundVars(std::move(boundVars))
    {}

    std::vector<Symbol*>& captures() { return _captures; }
    std::unordered_map<Symbol*, Symbol*>& replacements() { return _replacements; }

    virtual void visit(NullaryNode* node);
    virtual void visit(FunctionCallNode* node);

private:
    SymbolTable* _symbolTable;
    VariableSymbol* _envSymbol;

    std::unordered_set<Symbol*> _boundVars;
    std::unordered_map<Symbol*, Symbol*> _replacements;
    std::vector<Symbol*> _captures;
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
