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

class AstContext;

////// Abstract base nodes /////////////////////////////////////////////////////

class AstNode
{
public:
	AstNode(AstContext& context, const YYLTYPE& location);
	virtual ~AstNode();
	virtual void accept(AstVisitor* visitor) = 0;

	// For error reporting
	YYLTYPE location;

	// For semantic analysis
	std::shared_ptr<Type> type;

	// For code generation
	std::shared_ptr<Address> address;
};

#define AST_VISITABLE() virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

class StatementNode : public AstNode
{
public:
	StatementNode(AstContext& context, const YYLTYPE& location)
	: AstNode(context, location)
	{}
};

class ExpressionNode : public StatementNode
{
public:
	ExpressionNode(AstContext& context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}
};

class LoopNode : public StatementNode
{
public:
	LoopNode(AstContext& context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}
};

////// Miscellaneous AST nodes /////////////////////////////////////////////////

class TypeName : public AstNode
{
public:
    TypeName(AstContext& context, const YYLTYPE& location, const std::string& name)
    : AstNode(context, location), name(name)
    {}

    TypeName(AstContext& context, const YYLTYPE& location, const char* name)
    : AstNode(context, location), name(name)
    {}

    std::string str() const;

    AST_VISITABLE();

    std::string name;
    std::vector<TypeName*> parameters;

    // Annotations
    std::shared_ptr<Type> type;
};

class ConstructorSpec : public AstNode
{
public:
	ConstructorSpec(AstContext& context, const YYLTYPE& location, const std::string& name)
	: AstNode(context, location), name(name)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<TypeName*> members;

	// Annotations
	std::unordered_map<std::string, std::shared_ptr<Type>> typeContext;
	std::shared_ptr<Type> resultType;
	std::vector<std::shared_ptr<Type>> memberTypes;
	ValueConstructor* valueConstructor = nullptr;
};

////// Top-level nodes /////////////////////////////////////////////////////////

class ProgramNode : public AstNode
{
public:
	ProgramNode(AstContext& context, const YYLTYPE& location)
	: AstNode(context, location), scope(new Scope)
	{}

	AST_VISITABLE();

	std::vector<AstNode*> children;

	// Annotations
	std::shared_ptr<Scope> scope;
};

////// Expression nodes ////////////////////////////////////////////////////////

class BlockNode : public ExpressionNode
{
public:
	BlockNode(AstContext& context, const YYLTYPE& location)
	: ExpressionNode(context, location)
	{}

	AST_VISITABLE();

	std::vector<StatementNode*> children;
};

class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};

	LogicalNode(AstContext& context, const YYLTYPE& location, ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: ExpressionNode(context, location), lhs(lhs), op(op), rhs(rhs)
	{}

	AST_VISITABLE();

	ExpressionNode* lhs;
	Operator op;
	ExpressionNode* rhs;
};

class ComparisonNode : public ExpressionNode
{
public:
	enum Operator { kGreater, kEqual, kLess, kGreaterOrEqual, kLessOrEqual, kNotEqual };

	ComparisonNode(AstContext& context, const YYLTYPE& location, ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: ExpressionNode(context, location), lhs(lhs), op(op), rhs(rhs)
	{}

	AST_VISITABLE();

	ExpressionNode* lhs;
	Operator op;
	ExpressionNode* rhs;
};

class NullaryNode : public ExpressionNode
{
public:
	NullaryNode(AstContext& context, const YYLTYPE& location, const std::string& name)
	: ExpressionNode(context, location), name(name)
	{}

	AST_VISITABLE();

	std::string name;

	// Annotations
	Symbol* symbol = nullptr;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(AstContext& context, const YYLTYPE& location, long value)
	: ExpressionNode(context, location), value(value)
	{}

	AST_VISITABLE();

	long value;
};

class BoolNode : public ExpressionNode
{
public:
	BoolNode(AstContext& context, const YYLTYPE& location, bool value)
	: ExpressionNode(context, location), value(value)
	{}

	AST_VISITABLE();

	bool value;
};

// Syntactic sugar for list and string literals
FunctionCallNode* makeList(AstContext& context, const YYLTYPE& location, std::vector<ExpressionNode*>& elements);
FunctionCallNode* makeString(AstContext& context, const YYLTYPE& location, const std::string& s);

class FunctionCallNode : public ExpressionNode
{
public:
	FunctionCallNode(AstContext& context, const YYLTYPE& location, const std::string& target, std::vector<ExpressionNode*>&& arguments)
	: ExpressionNode(context, location), target(target), arguments(std::move(arguments))
	{}

	FunctionCallNode(AstContext& context, const YYLTYPE& location, const std::string& target, std::initializer_list<ExpressionNode*>&& args)
    : ExpressionNode(context, location), target(target), arguments(std::move(args))
    {
    }

	AST_VISITABLE();

	std::string target;
	std::vector<ExpressionNode*> arguments;

	// Annotations
	Symbol* symbol = nullptr;
};

class VariableNode : public ExpressionNode
{
public:
	VariableNode(AstContext& context, const YYLTYPE& location, const std::string& name)
	: ExpressionNode(context, location), name(name)
	{
	}

	AST_VISITABLE();

	std::string name;

	// Annotations
	Symbol* symbol = nullptr;
};

////// Statement nodes /////////////////////////////////////////////////////////

class IfNode : public StatementNode
{
public:
	IfNode(AstContext& context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: StatementNode(context, location), condition(condition), body(body)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;
};

class IfElseNode : public StatementNode
{
public:
	IfElseNode(AstContext& context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body, StatementNode* else_body)
	: StatementNode(context, location), condition(condition), body(body), else_body(else_body)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;
	StatementNode* else_body;
};

class WhileNode : public LoopNode
{
public:
	WhileNode(AstContext& context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: LoopNode(context, location), condition(condition), body(body)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;
};

class ForeverNode : public LoopNode
{
public:
	ForeverNode(AstContext& context, const YYLTYPE& location, StatementNode* body)
	: LoopNode(context, location), body(body)
	{}

	AST_VISITABLE();

	StatementNode* body;
};

class BreakNode : public StatementNode
{
public:
	BreakNode(AstContext& context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}

	AST_VISITABLE();
};

// For loops are implemented as pure syntactic sugar
BlockNode* makeForNode(AstContext& context, const YYLTYPE& location, const std::string& loopVar, ExpressionNode* list, StatementNode* body);

class AssignNode : public StatementNode
{
public:
	AssignNode(AstContext& context, const YYLTYPE& location, const std::string& target, ExpressionNode* value)
	: StatementNode(context, location), target(target), value(value)
	{
	}

	AST_VISITABLE();

	std::string target;
	ExpressionNode* value;

	// Annotations
	Symbol* symbol = nullptr;
};

class LetNode : public StatementNode
{
public:
	LetNode(AstContext& context, const YYLTYPE& location, const std::string& target, TypeName* typeName, ExpressionNode* value)
	: StatementNode(context, location), target(target), typeName(typeName), value(value)
	{}

	AST_VISITABLE();

	std::string target;
	TypeName* typeName;
	ExpressionNode* value;

	// Annotations
	Symbol* symbol = nullptr;
};

class FunctionDefNode : public StatementNode
{
public:
	FunctionDefNode(AstContext& context, const YYLTYPE& location, const std::string& name, StatementNode* body, const std::vector<std::string>& params, TypeName* typeName)
	: StatementNode(context, location), name(name), body(body), params(params), typeName(typeName), scope(new Scope)
	{}

	AST_VISITABLE();

	std::string name;
	StatementNode* body;
	std::vector<std::string> params;
	TypeName* typeName;

	// Annotations
	Symbol* symbol = nullptr;
	std::vector<Symbol*> parameterSymbols;
	std::shared_ptr<Scope> scope;
};

class MatchNode : public StatementNode
{
public:
	MatchNode(AstContext& context, const YYLTYPE& location, const std::string& constructor, const std::vector<std::string>& params, ExpressionNode* body)
	: StatementNode(context, location), constructor(constructor), params(params), body(body)
	{}

	AST_VISITABLE();

	std::string constructor;
	std::vector<std::string> params;
	ExpressionNode* body;

	// Annotations
	std::vector<Symbol*> symbols;
	Symbol* constructorSymbol = nullptr;
};

class MatchArm : public AstNode
{
public:
	MatchArm(AstContext& context, const YYLTYPE& location, const std::string& constructor, const std::vector<std::string>& params, StatementNode* body)
	: AstNode(context, location), constructor(constructor), params(params), body(body)
	{}

	AST_VISITABLE();

	std::string constructor;
	std::vector<std::string> params;
	StatementNode* body;

	// Annotations
	std::shared_ptr<Type> matchType;
	std::vector<Symbol*> symbols;
	Symbol* constructorSymbol = nullptr;
	size_t constructorTag;
	ValueConstructor* valueConstructor;
};

class SwitchNode : public StatementNode
{
public:
	SwitchNode(AstContext& context, const YYLTYPE& location, ExpressionNode* expr, std::vector<MatchArm*>&& arms)
	: StatementNode(context, location), expr(expr), arms(std::move(arms))
	{}

	AST_VISITABLE();

	ExpressionNode* expr;
	std::vector<MatchArm*> arms;
};

class DataDeclaration : public StatementNode
{
public:
	DataDeclaration(AstContext& context, const YYLTYPE& location, const std::string& name, const std::vector<std::string>& typeParameters, const std::vector<ConstructorSpec*>& specs)
	: StatementNode(context, location), name(name), typeParameters(typeParameters)
	{
		constructorSpecs = specs;
	}

	AST_VISITABLE();

	std::string name;
	std::vector<std::string> typeParameters;
	std::vector<ConstructorSpec*> constructorSpecs;

	// Annotations
	std::vector<ValueConstructor*> valueConstructors;
};

class TypeAliasNode : public StatementNode
{
public:
	TypeAliasNode(AstContext& context, const YYLTYPE& location, const std::string& name, TypeName* underlying)
	: StatementNode(context, location), name(name), underlying(underlying)
	{}

	AST_VISITABLE();

	std::string name;
	TypeName* underlying;
};

class ForeignDeclNode : public StatementNode
{
public:
	ForeignDeclNode(AstContext& context, const YYLTYPE& location, const std::string& name, const std::vector<std::string>& params, TypeName* typeName)
	: StatementNode(context, location), name(name), params(params), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<std::string> params;
	TypeName* typeName;

	// Annotations
	Symbol* symbol = nullptr;
};

class ReturnNode : public StatementNode
{
public:
	ReturnNode(AstContext& context, const YYLTYPE& location, ExpressionNode* expression)
	: StatementNode(context, location), expression(expression)
	{}

	AST_VISITABLE();

	ExpressionNode* expression;
};

//// Structures ////////////////////////////////////////////////////////////////

class MemberDefNode : public AstNode
{
public:
	MemberDefNode(AstContext& context, const YYLTYPE& location, const std::string& name, TypeName* typeName)
	: AstNode(context, location), name(name), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	TypeName* typeName;

	// Annotations
	std::shared_ptr<Type> memberType;
};

class StructDefNode : public StatementNode
{
public:
	StructDefNode(AstContext& context, const YYLTYPE& location, const std::string& name, const std::vector<MemberDefNode*>& members)
	: StatementNode(context, location), name(name), members(members)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<MemberDefNode*> members;

	// Annotations
	std::shared_ptr<Type> structType;
	ValueConstructor* valueConstructor;
};


class MemberAccessNode : public ExpressionNode
{
public:
	MemberAccessNode(AstContext& context, const YYLTYPE& location, const std::string& varName, const std::string& memberName)
	: ExpressionNode(context, location), varName(varName), memberName(memberName)
	{}

	AST_VISITABLE();

	std::string varName;
	std::string memberName;

	// Annotations
	VariableSymbol* varSymbol;
	MemberSymbol* memberSymbol;
	size_t memberLocation;
};



#endif
