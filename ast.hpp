#ifndef AST_HPP
#define AST_HPP

#include <cassert>
#include <iostream>
#include <list>
#include <memory>
#include <string>

#include "ast_visitor.hpp"
#include "scope.hpp"
#include "types.hpp"

struct YYLTYPE;

////// Abstract base nodes /////////////////////////////////////////////////////

class AstNode
{
public:
	AstNode();
	virtual ~AstNode();
	virtual void accept(AstVisitor* visitor) = 0;

	YYLTYPE* location;
	std::shared_ptr<Type> type;
};

class StatementNode : public AstNode {};
class ExpressionNode : public StatementNode {};
class AssignableNode : public ExpressionNode
{
public:
	virtual AssignableNode* clone() = 0;
};

////// Utility classes other than AST nodes ////////////////////////////////////

typedef std::vector<std::unique_ptr<ExpressionNode>> ArgList;
typedef std::vector<std::unique_ptr<TypeName>> TypeDecl;

class ConstructorSpec
{
public:
	ConstructorSpec(const std::string& name)
	: name(name)
	{}

	void append(TypeName* typeName) { members_.emplace_back(typeName); }

	const std::vector<std::unique_ptr<TypeName>>& members() const { return members_; }

	void setMemberTypes(const std::vector<std::shared_ptr<Type>> types)
	{
		assert(types.size() == members_.size());
		types_ = types;
	}
	const std::vector<std::shared_ptr<Type>>& memberTypes() { return types_; }

	std::string name;

private:
	std::vector<std::unique_ptr<TypeName>> members_;
	std::vector<std::shared_ptr<Type>> types_;
};


////// Top-level nodes /////////////////////////////////////////////////////////

class ProgramNode : public AstNode
{
public:
	ProgramNode()
	: scope(new Scope)
	{}

	void append(AstNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::shared_ptr<Scope> scope;
	std::list<std::unique_ptr<AstNode>> children;
};

////// Expression nodes ////////////////////////////////////////////////////////

class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};

	LogicalNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs(lhs), op(op), rhs(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> lhs;
	Operator op;
	std::unique_ptr<ExpressionNode> rhs;
};

class ComparisonNode : public ExpressionNode
{
public:
	enum Operator { kGreater, kEqual, kLess, kGreaterOrEqual, kLessOrEqual, kNotEqual };

	ComparisonNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs(lhs), op(op), rhs(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> lhs;
	Operator op;
	std::unique_ptr<ExpressionNode> rhs;
};

class NullaryNode : public ExpressionNode
{
public:
	NullaryNode(const std::string& name) : name(name), symbol(nullptr) {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	Symbol* symbol;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(long value) : value(value) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	long value;
};

class BoolNode : public ExpressionNode
{
public:
	BoolNode(bool value) : value(value) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	bool value;
};

// Syntactic sugar for list and string literals
FunctionCallNode* makeList(ArgList& elements);
FunctionCallNode* makeString(const std::string& s);

class FunctionCallNode : public ExpressionNode
{
public:
	FunctionCallNode(const std::string& target, ArgList&& arguments)
	: target(target), arguments(std::move(arguments)), symbol(nullptr)
	{}

	FunctionCallNode(const std::string& target, std::initializer_list<ExpressionNode*> args)
    : target(target), symbol(nullptr)
    {
    	for (auto& arg : args)
    	{
    		arguments.emplace_back(arg);
    	}
    }

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string target;
	ArgList arguments;
	Symbol* symbol;
};

class VariableNode : public AssignableNode
{
public:
	VariableNode(const std::string& name)
	: name(name), symbol(nullptr)
	{
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	virtual VariableNode* clone() { return new VariableNode(*this); }

	std::string name;
	Symbol* symbol;
};

////// Statement nodes /////////////////////////////////////////////////////////

class BlockNode : public StatementNode
{
public:
	void append(StatementNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::list<std::unique_ptr<StatementNode>> children;
};

class IfNode : public StatementNode
{
public:
	IfNode(ExpressionNode* condition, StatementNode* body) : condition(condition), body(body) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> condition;
	std::unique_ptr<StatementNode> body;
};

class IfElseNode : public StatementNode
{
public:
	IfElseNode(ExpressionNode* condition, StatementNode* body, StatementNode* else_body)
	: condition(condition), body(body), else_body(else_body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> condition;
	std::unique_ptr<StatementNode> body;
	std::unique_ptr<StatementNode> else_body;
};

class WhileNode : public StatementNode
{
public:
	WhileNode(ExpressionNode* condition, StatementNode* body) : condition(condition), body(body) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> condition;
	std::unique_ptr<StatementNode> body;
};

class BreakNode : public StatementNode
{
public:
	BreakNode() {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	WhileNode* loop;
};

// For loops are implemented as pure syntactic sugar
BlockNode* makeForNode(const std::string& loopVar, ExpressionNode* list, StatementNode* body);

class AssignNode : public StatementNode
{
public:
	AssignNode(AssignableNode* target, ExpressionNode* value)
	: target(target), value(value)
	{
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<AssignableNode> target;
	std::unique_ptr<ExpressionNode> value;
};

class LetNode : public StatementNode
{
public:
	LetNode(const std::string& target, TypeName* typeName, ExpressionNode* value)
	: target(target), typeName(typeName), value(value), symbol(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string target;
	std::unique_ptr<TypeName> typeName;
	std::unique_ptr<ExpressionNode> value;
	Symbol* symbol;
};

typedef std::vector<std::string> ParamList;

class FunctionDefNode : public StatementNode
{
public:
	FunctionDefNode(const std::string& name, StatementNode* body, ParamList* params, TypeDecl* typeDecl)
	: name(name), body(body), params(params), typeDecl(typeDecl), symbol(nullptr), scope(new Scope)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<StatementNode> body;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<TypeDecl> typeDecl;
	Symbol* symbol;
	std::vector<Symbol*> parameterSymbols;
	std::shared_ptr<Scope> scope;
};

class MatchNode : public StatementNode
{
public:
	MatchNode(const std::string& constructor, ParamList* params, ExpressionNode* body)
	: constructor(constructor), params(params), body(body), constructorSymbol(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	void attachSymbol(Symbol* symbol) { symbols.push_back(symbol); }

	std::string constructor;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<ExpressionNode> body;
	std::vector<Symbol*> symbols;
	Symbol* constructorSymbol;

};

class DataDeclaration : public StatementNode
{
public:
	DataDeclaration(const std::string& name, ConstructorSpec* constructor)
	: name(name), constructor(constructor)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<ConstructorSpec> constructor;
	ValueConstructor* valueConstructor;
};

class TypeAliasNode : public StatementNode
{
public:
	TypeAliasNode(const std::string& name, TypeName* underlying)
	: name(name), underlying(underlying)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<TypeName> underlying;
};

class ForeignDeclNode : public StatementNode
{
public:
	ForeignDeclNode(const std::string& name, ParamList* params, TypeDecl* typeDecl)
	: name(name), params(params), typeDecl(typeDecl), symbol(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<TypeDecl> typeDecl;
	Symbol* symbol;
	std::vector<const Type*> paramTypes;
};

class ReturnNode : public StatementNode
{
public:
	ReturnNode(ExpressionNode* expression) : expression(expression) {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> expression;
};

//// Structures ////////////////////////////////////////////////////////////////

class MemberDefNode : public AstNode
{
public:
	MemberDefNode(const std::string& name, TypeName* typeName)
	: name(name), typeName(typeName)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<TypeName> typeName;
	std::shared_ptr<Type> memberType;
};

typedef std::vector<std::unique_ptr<MemberDefNode>> MemberList;

class StructDefNode : public StatementNode
{
public:
	StructDefNode(const std::string& name, MemberList* members)
	: name(name), members(members)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<MemberList> members;
	std::shared_ptr<Type> structType;
};

class StructInitNode : public ExpressionNode
{
public:
	StructInitNode(const std::string& structName)
	: structName(structName)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string structName;
};

class MemberAccessNode : public AssignableNode
{
public:
	MemberAccessNode(const std::string& varName, const std::string& memberName)
	: varName(varName), memberName(memberName)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	virtual MemberAccessNode* clone() { return new MemberAccessNode(*this); }

	std::string varName;
	std::string memberName;
	Symbol* symbol;
	size_t memberLocation;
};

#endif
