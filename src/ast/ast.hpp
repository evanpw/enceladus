#ifndef AST_HPP
#define AST_HPP

#include "ast/ast_visitor.hpp"
#include "ir/value.hpp"
#include "parser/tokens.hpp"
#include "semantic/scope.hpp"
#include "semantic/types.hpp"

#include <cassert>
#include <iostream>
#include <list>
#include <memory>
#include <string>

class AstContext;

////// Abstract base nodes /////////////////////////////////////////////////////

class AstNode
{
public:
	AstNode(AstContext* context, const YYLTYPE& location);
	virtual ~AstNode();
	virtual void accept(AstVisitor* visitor) = 0;

	// For error reporting
	YYLTYPE location;

	// For semantic analysis
	Type* type;

	// For code generation
	Value* value = nullptr;
};

#define AST_VISITABLE() virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

class StatementNode : public AstNode
{
public:
	StatementNode(AstContext* context, const YYLTYPE& location)
	: AstNode(context, location)
	{}
};

class ExpressionNode : public StatementNode
{
public:
	ExpressionNode(AstContext* context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}
};

class LoopNode : public StatementNode
{
public:
	LoopNode(AstContext* context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}
};

////// Miscellaneous AST nodes /////////////////////////////////////////////////

class TypeName : public AstNode
{
public:
    TypeName(AstContext* context, const YYLTYPE& location, const std::string& name)
    : AstNode(context, location), name(name)
    {}

    TypeName(AstContext* context, const YYLTYPE& location, const char* name)
    : AstNode(context, location), name(name)
    {}

    std::string str() const;

    AST_VISITABLE();

    std::string name;
    std::vector<TypeName*> parameters;

    // Annotations
    Type* type;
};

class ConstructorSpec : public AstNode
{
public:
	ConstructorSpec(AstContext* context, const YYLTYPE& location, const std::string& name)
	: AstNode(context, location), name(name)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<TypeName*> members;

	// Annotations
	std::unordered_map<std::string, Type*> typeContext;
	Type* resultType;
	std::vector<Type*> memberTypes;
	ValueConstructor* valueConstructor;
};

////// Top-level nodes /////////////////////////////////////////////////////////

class ProgramNode : public AstNode
{
public:
	ProgramNode(AstContext* context, const YYLTYPE& location)
	: AstNode(context, location)
	{}

	AST_VISITABLE();

	std::vector<AstNode*> children;

	// Annotations
	Scope scope;
};

////// Expression nodes ////////////////////////////////////////////////////////

class BlockNode : public ExpressionNode
{
public:
	BlockNode(AstContext* context, const YYLTYPE& location)
	: ExpressionNode(context, location)
	{}

	AST_VISITABLE();

	std::vector<StatementNode*> children;
};

class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};

	LogicalNode(AstContext* context, const YYLTYPE& location, ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
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

	ComparisonNode(AstContext* context, const YYLTYPE& location, ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
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
	NullaryNode(AstContext* context, const YYLTYPE& location, const std::string& name)
	: ExpressionNode(context, location), name(name)
	{}

	AST_VISITABLE();

	std::string name;

	// Annotations
	enum NullaryKind { VARIABLE, FUNC_CALL, FOREIGN_CALL, CLOSURE };
	Symbol* symbol = nullptr;
	NullaryKind kind;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(AstContext* context, const YYLTYPE& location, int64_t intValue)
	: ExpressionNode(context, location), intValue(intValue)
	{}

	AST_VISITABLE();

	int64_t intValue;
};

class BoolNode : public ExpressionNode
{
public:
	BoolNode(AstContext* context, const YYLTYPE& location, bool boolValue)
	: ExpressionNode(context, location), boolValue(boolValue)
	{}

	AST_VISITABLE();

	bool boolValue;
};

// Syntactic sugar for list and string literals
FunctionCallNode* makeList(AstContext* context, const YYLTYPE& location, std::vector<ExpressionNode*>& elements);

class StringLiteralNode : public ExpressionNode
{
public:
	StringLiteralNode(AstContext* context, const YYLTYPE& location, const std::string& content)
	: ExpressionNode(context, location), content(content), counter(nextCounter++)
	{
	}

	AST_VISITABLE();

	std::string content;
	int counter;

	// Annotations
	Symbol* symbol = nullptr;

	static int nextCounter;
};

class FunctionCallNode : public ExpressionNode
{
public:
	FunctionCallNode(AstContext* context, const YYLTYPE& location, const std::string& target, std::vector<ExpressionNode*>&& arguments)
	: ExpressionNode(context, location), target(target), arguments(std::move(arguments))
	{}

	FunctionCallNode(AstContext* context, const YYLTYPE& location, const std::string& target, std::initializer_list<ExpressionNode*>&& args)
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
	VariableNode(AstContext* context, const YYLTYPE& location, const std::string& name)
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
	IfNode(AstContext* context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: StatementNode(context, location), condition(condition), body(body)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;

	// Annotations
	Scope bodyScope;
};

class IfElseNode : public StatementNode
{
public:
	IfElseNode(AstContext* context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body, StatementNode* elseBody)
	: StatementNode(context, location), condition(condition), body(body), elseBody(elseBody)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;
	StatementNode* elseBody;

	// Annotations
	Scope bodyScope;
	Scope elseScope;
};

class WhileNode : public LoopNode
{
public:
	WhileNode(AstContext* context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: LoopNode(context, location), condition(condition), body(body)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;

	// Annotations
	Scope bodyScope;
};

class ForeachNode : public LoopNode
{
public:
	ForeachNode(AstContext* context, const YYLTYPE& location, const std::string& varName, ExpressionNode* listExpression, StatementNode* body)
	: LoopNode(context, location), varName(varName), listExpression(listExpression), body(body)
	{}

	AST_VISITABLE();

	std::string varName;
	ExpressionNode* listExpression;
	StatementNode* body;

	// Annotations
	Symbol* symbol;
	Scope bodyScope;

	// HACK: give the code generator to these functions
	Symbol* headSymbol;
	Symbol* tailSymbol;
	Symbol* nullSymbol;
};

class ForNode : public LoopNode
{
public:
	ForNode(AstContext* context, const YYLTYPE& location, const std::string& varName, ExpressionNode* fromExpression, ExpressionNode* toExpression, StatementNode* body)
	: LoopNode(context, location), varName(varName), fromExpression(fromExpression), toExpression(toExpression), body(body)
	{}

	AST_VISITABLE();

	std::string varName;
	ExpressionNode* fromExpression;
	ExpressionNode* toExpression;
	StatementNode* body;

	// Annotations
	Symbol* symbol;
	Scope bodyScope;
};

class ForeverNode : public LoopNode
{
public:
	ForeverNode(AstContext* context, const YYLTYPE& location, StatementNode* body)
	: LoopNode(context, location), body(body)
	{}

	AST_VISITABLE();

	StatementNode* body;

	// Annotations
	Scope bodyScope;
};

class BreakNode : public StatementNode
{
public:
	BreakNode(AstContext* context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}

	AST_VISITABLE();
};

class AssignNode : public StatementNode
{
public:
	AssignNode(AstContext* context, const YYLTYPE& location, const std::string& target, ExpressionNode* rhs)
	: StatementNode(context, location), target(target), rhs(rhs)
	{
	}

	AST_VISITABLE();

	std::string target;
	ExpressionNode* rhs;

	// Annotations
	Symbol* symbol = nullptr;
};

class VariableDefNode : public StatementNode
{
public:
	VariableDefNode(AstContext* context, const YYLTYPE& location, const std::string& target, TypeName* typeName, ExpressionNode* rhs)
	: StatementNode(context, location), target(target), typeName(typeName), rhs(rhs)
	{}

	AST_VISITABLE();

	std::string target;
	TypeName* typeName;
	ExpressionNode* rhs;

	// Annotations
	Symbol* symbol = nullptr;
};

class FunctionDefNode : public StatementNode
{
public:
	FunctionDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, StatementNode* body, const std::vector<std::string>& typeParams, const std::vector<std::string>& params, TypeName* typeName)
	: StatementNode(context, location), name(name), body(body), typeParams(typeParams), params(params), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	StatementNode* body;
	std::vector<std::string> typeParams;
	std::vector<std::string> params;
	TypeName* typeName;

	// Annotations
	Symbol* symbol = nullptr;
	std::vector<Symbol*> parameterSymbols;
	Scope scope;
	FunctionType* functionType;
};

class LetNode : public StatementNode
{
public:
	LetNode(AstContext* context, const YYLTYPE& location, const std::string& constructor, const std::vector<std::string>& params, ExpressionNode* body)
	: StatementNode(context, location), constructor(constructor), params(params), body(body)
	{}

	AST_VISITABLE();

	std::string constructor;
	std::vector<std::string> params;
	ExpressionNode* body;

	// Annotations
	std::vector<Symbol*> symbols;
	ValueConstructor* valueConstructor;
};

class MatchArm : public AstNode
{
public:
	MatchArm(AstContext* context, const YYLTYPE& location, const std::string& constructor, const std::vector<std::string>& params, StatementNode* body)
	: AstNode(context, location), constructor(constructor), params(params), body(body)
	{}

	AST_VISITABLE();

	std::string constructor;
	std::vector<std::string> params;
	StatementNode* body;

	// Annotations
	Type* matchType;
	std::vector<Symbol*> symbols;
	size_t constructorTag;
	ValueConstructor* valueConstructor;
	Scope bodyScope;
};

class MatchNode : public StatementNode
{
public:
	MatchNode(AstContext* context, const YYLTYPE& location, ExpressionNode* expr, std::vector<MatchArm*>&& arms)
	: StatementNode(context, location), expr(expr), arms(std::move(arms))
	{}

	AST_VISITABLE();

	ExpressionNode* expr;
	std::vector<MatchArm*> arms;
};

class DataDeclaration : public StatementNode
{
public:
	DataDeclaration(AstContext* context, const YYLTYPE& location, const std::string& name, const std::vector<std::string>& typeParameters, const std::vector<ConstructorSpec*>& specs)
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
	TypeAliasNode(AstContext* context, const YYLTYPE& location, const std::string& name, TypeName* underlying)
	: StatementNode(context, location), name(name), underlying(underlying)
	{}

	AST_VISITABLE();

	std::string name;
	TypeName* underlying;
};

class ForeignDeclNode : public StatementNode
{
public:
	ForeignDeclNode(AstContext* context, const YYLTYPE& location, const std::string& name, const std::vector<std::string>& typeParams, const std::vector<std::string>& params, TypeName* typeName)
	: StatementNode(context, location), name(name), typeParams(typeParams), params(params), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<std::string> typeParams;
	std::vector<std::string> params;
	TypeName* typeName;

	// Annotations
	Symbol* symbol = nullptr;
};

class ReturnNode : public StatementNode
{
public:
	ReturnNode(AstContext* context, const YYLTYPE& location, ExpressionNode* expression)
	: StatementNode(context, location), expression(expression)
	{}

	AST_VISITABLE();

	ExpressionNode* expression;
};

//// Structures ////////////////////////////////////////////////////////////////

class MemberDefNode : public AstNode
{
public:
	MemberDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, TypeName* typeName)
	: AstNode(context, location), name(name), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	TypeName* typeName;

	// Annotations
	Type* memberType;
};

class StructDefNode : public StatementNode
{
public:
	StructDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, const std::vector<MemberDefNode*>& members)
	: StatementNode(context, location), name(name), members(members)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<MemberDefNode*> members;

	// Annotations
	Type* structType;
	ValueConstructor* valueConstructor;
};


class MemberAccessNode : public ExpressionNode
{
public:
	MemberAccessNode(AstContext* context, const YYLTYPE& location, const std::string& varName, const std::string& memberName)
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