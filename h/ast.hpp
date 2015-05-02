#ifndef AST_HPP
#define AST_HPP

#include <cassert>
#include <iostream>
#include <list>
#include <memory>
#include <string>

#include "address.hpp"
#include "ast_visitor.hpp"
#include "scope.hpp"
#include "tokens.hpp"
#include "types.hpp"

////// Abstract base nodes /////////////////////////////////////////////////////

class AstNode
{
public:
	AstNode(const YYLTYPE& location);
	virtual ~AstNode();
	virtual void accept(AstVisitor* visitor) = 0;

	// For error reporting
	YYLTYPE location;

	// For semantic analysis
	std::shared_ptr<Type> type;

	// For code generation
	std::shared_ptr<Address> address;
};

class StatementNode : public AstNode
{
public:
	StatementNode(const YYLTYPE& location)
	: AstNode(location)
	{}
};

class ExpressionNode : public StatementNode
{
public:
	ExpressionNode(const YYLTYPE& location)
	: StatementNode(location)
	{}
};

class LoopNode : public StatementNode
{
public:
	LoopNode(const YYLTYPE& location)
	: StatementNode(location)
	{}
};

////// Utility classes other than AST nodes ////////////////////////////////////

typedef std::vector<std::unique_ptr<ExpressionNode>> ArgList;

class TypeName
{
public:
    TypeName(const std::string& name, const YYLTYPE& location)
    : _name(name), _location(location)
    {}

    TypeName(const char* name, const YYLTYPE& location)
    : _name(name), _location(location)
    {}

    const std::string& name() const { return _name; }
    const YYLTYPE& location() const { return _location; }

    const std::vector<std::unique_ptr<TypeName>>& parameters() const { return _parameters; }

    std::string str() const;

    void append(TypeName* parameter) { _parameters.emplace_back(parameter); }

private:
    std::string _name;
    std::vector<std::unique_ptr<TypeName>> _parameters;
    YYLTYPE _location;
};

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
	ProgramNode(const YYLTYPE& location)
	: AstNode(location), scope(new Scope)
	{}

	void append(AstNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::shared_ptr<Scope> scope;
	std::list<std::unique_ptr<AstNode>> children;
};

////// Expression nodes ////////////////////////////////////////////////////////

class BlockNode : public ExpressionNode
{
public:
	BlockNode(const YYLTYPE& location)
	: ExpressionNode(location)
	{}

	void append(StatementNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::vector<std::unique_ptr<StatementNode>> children;
};

class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};

	LogicalNode(const YYLTYPE& location, ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: ExpressionNode(location), lhs(lhs), op(op), rhs(rhs)
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

	ComparisonNode(const YYLTYPE& location, ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: ExpressionNode(location), lhs(lhs), op(op), rhs(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> lhs;
	Operator op;
	std::unique_ptr<ExpressionNode> rhs;
};

class NullaryNode : public ExpressionNode
{
public:
	NullaryNode(const YYLTYPE& location, const std::string& name)
	: ExpressionNode(location), name(name), symbol(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	Symbol* symbol;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(const YYLTYPE& location, long value)
	: ExpressionNode(location), value(value)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	long value;
};

class BoolNode : public ExpressionNode
{
public:
	BoolNode(const YYLTYPE& location, bool value)
	: ExpressionNode(location), value(value)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	bool value;
};

// Syntactic sugar for list and string literals
FunctionCallNode* makeList(const YYLTYPE& location, ArgList& elements);
FunctionCallNode* makeString(const YYLTYPE& location, const std::string& s);

class FunctionCallNode : public ExpressionNode
{
public:
	FunctionCallNode(const YYLTYPE& location, const std::string& target, ArgList&& arguments)
	: ExpressionNode(location), target(target), arguments(std::move(arguments)), symbol(nullptr)
	{}

	FunctionCallNode(const YYLTYPE& location, const std::string& target, std::initializer_list<ExpressionNode*> args)
    : ExpressionNode(location), target(target), symbol(nullptr)
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

class VariableNode : public ExpressionNode
{
public:
	VariableNode(const YYLTYPE& location, const std::string& name)
	: ExpressionNode(location), name(name), symbol(nullptr)
	{
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	Symbol* symbol;
};

////// Statement nodes /////////////////////////////////////////////////////////

class IfNode : public StatementNode
{
public:
	IfNode(const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: StatementNode(location), condition(condition), body(body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> condition;
	std::unique_ptr<StatementNode> body;
};

class IfElseNode : public StatementNode
{
public:
	IfElseNode(const YYLTYPE& location, ExpressionNode* condition, StatementNode* body, StatementNode* else_body)
	: StatementNode(location), condition(condition), body(body), else_body(else_body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> condition;
	std::unique_ptr<StatementNode> body;
	std::unique_ptr<StatementNode> else_body;
};

class WhileNode : public LoopNode
{
public:
	WhileNode(const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: LoopNode(location), condition(condition), body(body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> condition;
	std::unique_ptr<StatementNode> body;
};

class ForeverNode : public LoopNode
{
public:
	ForeverNode(const YYLTYPE& location, StatementNode* body)
	: LoopNode(location), body(body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<StatementNode> body;
};

class BreakNode : public StatementNode
{
public:
	BreakNode(const YYLTYPE& location)
	: StatementNode(location)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }
};

// For loops are implemented as pure syntactic sugar
BlockNode* makeForNode(const YYLTYPE& location, const std::string& loopVar, ExpressionNode* list, StatementNode* body);

class AssignNode : public StatementNode
{
public:
	AssignNode(const YYLTYPE& location, const std::string& target, ExpressionNode* value)
	: StatementNode(location), target(target), value(value)
	{
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string target;
	std::unique_ptr<ExpressionNode> value;
	Symbol* symbol;
};

class LetNode : public StatementNode
{
public:
	LetNode(const YYLTYPE& location, const std::string& target, TypeName* typeName, ExpressionNode* value)
	: StatementNode(location), target(target), typeName(typeName), value(value), symbol(nullptr)
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
	FunctionDefNode(const YYLTYPE& location, const std::string& name, StatementNode* body, ParamList* params, TypeName* typeName)
	: StatementNode(location), name(name), body(body), params(params), typeName(typeName), symbol(nullptr), scope(new Scope)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<StatementNode> body;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<TypeName> typeName;
	Symbol* symbol;
	std::vector<Symbol*> parameterSymbols;
	std::shared_ptr<Scope> scope;
};

class MatchNode : public StatementNode
{
public:
	MatchNode(const YYLTYPE& location, const std::string& constructor, ParamList* params, ExpressionNode* body)
	: StatementNode(location), constructor(constructor), params(params), body(body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	void attachSymbol(Symbol* symbol) { symbols.push_back(symbol); }

	std::string constructor;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<ExpressionNode> body;
	std::vector<Symbol*> symbols;
	Symbol* constructorSymbol = nullptr;
};

class MatchArm : public AstNode
{
public:
	MatchArm(const YYLTYPE& location, const std::string& constructor, ParamList* params, StatementNode* body)
	: AstNode(location), constructor(constructor), params(params), body(body)
	{}

	std::string constructor;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<StatementNode> body;

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	void attachSymbol(Symbol* symbol) { symbols.push_back(symbol); }

	std::vector<Symbol*> symbols;
	Symbol* constructorSymbol = nullptr;
};

class SwitchNode : public StatementNode
{
public:
	SwitchNode(const YYLTYPE& location, ExpressionNode* expr, std::vector<std::unique_ptr<MatchArm>>&& arms)
	: StatementNode(location), expr(expr), arms(std::move(arms))
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> expr;
	std::vector<std::unique_ptr<MatchArm>> arms;
};

class DataDeclaration : public StatementNode
{
public:
	DataDeclaration(const YYLTYPE& location, const std::string& name, const std::vector<std::string>& typeParameters, const std::vector<ConstructorSpec*> specs)
	: StatementNode(location), name(name), typeParameters(typeParameters)
	{
		for (auto& spec : specs)
		{
			constructorSpecs.emplace_back(spec);
		}
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::vector<std::string> typeParameters;
	std::vector<std::unique_ptr<ConstructorSpec>> constructorSpecs;
	std::vector<ValueConstructor*> valueConstructors;
};

class TypeAliasNode : public StatementNode
{
public:
	TypeAliasNode(const YYLTYPE& location, const std::string& name, TypeName* underlying)
	: StatementNode(location), name(name), underlying(underlying)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<TypeName> underlying;
};

class ForeignDeclNode : public StatementNode
{
public:
	ForeignDeclNode(const YYLTYPE& location, const std::string& name, ParamList* params, TypeName* typeName)
	: StatementNode(location), name(name), params(params), typeName(typeName), symbol(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<ParamList> params;
	std::unique_ptr<TypeName> typeName;
	Symbol* symbol;
	std::vector<const Type*> paramTypes;
};

class ReturnNode : public StatementNode
{
public:
	ReturnNode(const YYLTYPE& location, ExpressionNode* expression)
	: StatementNode(location), expression(expression)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::unique_ptr<ExpressionNode> expression;
};

//// Structures ////////////////////////////////////////////////////////////////

class MemberDefNode : public AstNode
{
public:
	MemberDefNode(const YYLTYPE& location, const std::string& name, TypeName* typeName)
	: AstNode(location), name(name), typeName(typeName)
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
	StructDefNode(const YYLTYPE& location, const std::string& name, MemberList* members)
	: StatementNode(location), name(name), members(members)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string name;
	std::unique_ptr<MemberList> members;
	std::shared_ptr<Type> structType;
	ValueConstructor* valueConstructor;
};


class MemberAccessNode : public ExpressionNode
{
public:
	MemberAccessNode(const YYLTYPE& location, const std::string& varName, const std::string& memberName)
	: ExpressionNode(location), varName(varName), memberName(memberName)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	std::string varName;
	std::string memberName;
	VariableSymbol* varSymbol;
	MemberSymbol* memberSymbol;
	size_t memberLocation;
};

#endif
