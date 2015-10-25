#ifndef AST_HPP
#define AST_HPP

#include "ast/ast_visitor.hpp"
#include "ir/value.hpp"
#include "parser/tokens.hpp"
#include "semantic/symbol.hpp"
#include "semantic/types.hpp"

#include <cassert>
#include <iostream>
#include <list>
#include <map>
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
#define AST_UNVISITABLE() virtual void accept(AstVisitor* visitor) { assert(false); }

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


////// Utility classes /////////////////////////////////////////////////////////

struct TypeParam
{
	TypeParam(const std::string& name)
	: name(name)
	{}

	std::string name;
	std::vector<TypeName*> constraints;
};


////// Miscellaneous AST nodes /////////////////////////////////////////////////

class TypeName : public AstNode
{
public:
    TypeName(AstContext* context, const YYLTYPE& location, const std::string& name)
    : AstNode(context, location), name(name)
    {}

    std::string str() const;

    AST_UNVISITABLE();

    std::string name;
    std::vector<TypeName*> parameters;
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
	size_t constructorTag;
	std::unordered_map<std::string, Type*> typeContext;
	Type* resultType;
	std::vector<Type*> memberTypes;
	ValueConstructor* valueConstructor;
	ConstructorSymbol* symbol;
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
	enum NullaryKind { VARIABLE, FUNC_CALL, CLOSURE };
	Symbol* symbol = nullptr;
	std::map<TypeVariable*, Type*> typeAssignment;
	NullaryKind kind;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(AstContext* context, const YYLTYPE& location, int64_t intValue, const std::string& suffix)
	: ExpressionNode(context, location), intValue(intValue), suffix(suffix)
	{}

	AST_VISITABLE();

	int64_t intValue;
	std::string suffix;
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
FunctionCallNode* createList(AstContext* context, const YYLTYPE& location, std::vector<ExpressionNode*>& elements);

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
	FunctionCallNode(AstContext* context, const YYLTYPE& location, const std::string& target, std::vector<ExpressionNode*>&& arguments, TypeName* typeName = nullptr)
	: ExpressionNode(context, location), target(target), arguments(std::move(arguments)), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string target;
	std::vector<ExpressionNode*> arguments;
	TypeName* typeName;

	// Annotations
	Symbol* symbol = nullptr;
	std::map<TypeVariable*, Type*> typeAssignment;
};

class MethodCallNode : public ExpressionNode
{
public:
	MethodCallNode(AstContext* context, const YYLTYPE& location, ExpressionNode* object, const std::string& methodName, std::vector<ExpressionNode*>&& arguments)
	: ExpressionNode(context, location), object(object), methodName(methodName), arguments(std::move(arguments))
	{}

	AST_VISITABLE();

	ExpressionNode* object;
	std::string methodName;
	std::vector<ExpressionNode*> arguments;

	// Annotations
	Symbol* symbol = nullptr;
	std::map<TypeVariable*, Type*> typeAssignment;
};

class BinopNode : public ExpressionNode
{
public:
	enum Op {kAdd, kSub, kMul, kDiv, kMod};

	BinopNode(AstContext* context, const YYLTYPE& location, ExpressionNode* lhs, Op op, ExpressionNode* rhs)
	: ExpressionNode(context, location), lhs(lhs), op(op), rhs(rhs)
	{
	}

	AST_VISITABLE();

	ExpressionNode* lhs;
	Op op;
	ExpressionNode* rhs;
};

class CastNode : public ExpressionNode
{
public:
	CastNode(AstContext* context, const YYLTYPE& location, ExpressionNode* lhs, TypeName* typeName)
	: ExpressionNode(context, location), lhs(lhs), typeName(typeName)
	{
	}

	AST_VISITABLE();

	ExpressionNode* lhs;
	TypeName* typeName;
};

////// Statement nodes /////////////////////////////////////////////////////////

class PassNode : public StatementNode
{
public:
	PassNode(AstContext* context, const YYLTYPE& location)
	: StatementNode(context, location)
	{}

	AST_VISITABLE();
};

class IfNode : public StatementNode
{
public:
	IfNode(AstContext* context, const YYLTYPE& location, ExpressionNode* condition, StatementNode* body)
	: StatementNode(context, location), condition(condition), body(body)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;
	StatementNode* body;
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
};

class AssertNode : public StatementNode
{
public:
	AssertNode(AstContext* context, const YYLTYPE& location, ExpressionNode* condition)
	: StatementNode(context, location), condition(condition)
	{}

	AST_VISITABLE();

	ExpressionNode* condition;

	// Annotations
	FunctionSymbol* panicSymbol; // HACK
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
	Type* varType;

	// HACK: give the code generator to these functions
	MethodSymbol* headSymbol;
	std::map<TypeVariable*, Type*> headTypeAssignment;
	MethodSymbol* tailSymbol;
	std::map<TypeVariable*, Type*> tailTypeAssignment;
	MethodSymbol* emptySymbol;
	std::map<TypeVariable*, Type*> emptyTypeAssignment;
};


class ForeverNode : public LoopNode
{
public:
	ForeverNode(AstContext* context, const YYLTYPE& location, StatementNode* body)
	: LoopNode(context, location), body(body)
	{}

	AST_VISITABLE();

	StatementNode* body;
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
	AssignNode(AstContext* context, const YYLTYPE& location, ExpressionNode* lhs, ExpressionNode* rhs)
	: StatementNode(context, location), lhs(lhs), rhs(rhs)
	{
	}

	AST_VISITABLE();

	ExpressionNode* lhs;
	ExpressionNode* rhs;

	// Annotations
	Symbol* symbol = nullptr;
};

class VariableDefNode : public StatementNode
{
public:
	VariableDefNode(AstContext* context, const YYLTYPE& location, const std::string& target, ExpressionNode* rhs)
	: StatementNode(context, location), target(target), rhs(rhs)
	{}

	AST_VISITABLE();

	std::string target;
	ExpressionNode* rhs;

	// Annotations
	Symbol* symbol = nullptr;
};

class FunctionDefNode : public StatementNode
{
public:
	FunctionDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, StatementNode* body, std::vector<TypeParam>&& typeParams, const std::vector<std::string>& params, TypeName* typeName)
	: StatementNode(context, location), name(name), body(body), typeParams(typeParams), params(params), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	StatementNode* body;
	std::vector<TypeParam> typeParams;
	std::vector<std::string> params;
	TypeName* typeName;

	// Annotations
	Symbol* symbol = nullptr;
	std::vector<Symbol*> parameterSymbols;
	FunctionType* functionType;
};

class MethodDefNode : public FunctionDefNode
{
public:
	MethodDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, StatementNode* body, std::vector<TypeParam>&& typeParams, const std::vector<std::string>& params, TypeName* typeName)
	: FunctionDefNode(context, location, name, body, std::move(typeParams), params, typeName)
	{
	}

	AST_VISITABLE();

	// Annotations
	bool firstPassFinished = false;  // semantic analysis takes two passes for method definitions
};

class ImplNode : public StatementNode
{
public:
	ImplNode(AstContext* context, const YYLTYPE& location, std::vector<TypeParam>&& typeParams, TypeName* typeName, std::vector<MethodDefNode*>&& methods, TypeName* traitName)
	: StatementNode(context, location), typeParams(typeParams), typeName(typeName), methods(methods), traitName(traitName)
	{}

	AST_VISITABLE();

	std::vector<TypeParam> typeParams;
	TypeName* typeName;
	std::vector<MethodDefNode*> methods;
	TypeName* traitName;

	// Annotations
	std::unordered_map<std::string, Type*> typeContext;
};

class TraitMethodNode : public StatementNode
{
public:
	TraitMethodNode(AstContext* context, const YYLTYPE& location, const std::string& name, std::vector<std::string>&& params, TypeName* typeName)
	: StatementNode(context, location), name(name), params(params), typeName(typeName)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<std::string> params;
	TypeName* typeName;
};

class TraitDefNode : public StatementNode
{
public:
	TraitDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, std::vector<std::string>&& typeParams, std::vector<TraitMethodNode*>&& methods)
	: StatementNode(context, location), name(name), typeParams(typeParams), methods(methods)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<std::string> typeParams;
	std::vector<TraitMethodNode*> methods;

	// Annotations
	TraitSymbol* traitSymbol = nullptr;
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
	ConstructorSymbol* constructorSymbol;
	std::map<TypeVariable*, Type*> typeAssignment;

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
	ConstructorSymbol* constructorSymbol;
	std::map<TypeVariable*, Type*> typeAssignment;

	Type* matchType;
	std::vector<Symbol*> symbols;
	size_t constructorTag;
	ValueConstructor* valueConstructor;
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
	std::vector<ConstructorSymbol*> constructorSymbols;
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
	std::unordered_map<std::string, Type*> typeContext;
};

class StructDefNode : public StatementNode
{
public:
	StructDefNode(AstContext* context, const YYLTYPE& location, const std::string& name, std::vector<MemberDefNode*>&& members, std::vector<TypeParam>&& typeParameters)
	: StatementNode(context, location), name(name), members(members), typeParameters(typeParameters)
	{}

	AST_VISITABLE();

	std::string name;
	std::vector<MemberDefNode*> members;
	std::vector<TypeParam> typeParameters;

	// Annotations
	Type* structType;
	ValueConstructor* valueConstructor;
	ConstructorSymbol* constructorSymbol;
};


class MemberAccessNode : public ExpressionNode
{
public:
	MemberAccessNode(AstContext* context, const YYLTYPE& location, ExpressionNode* object, const std::string& memberName)
	: ExpressionNode(context, location), object(object), memberName(memberName)
	{}

	AST_VISITABLE();

	ExpressionNode* object;
	std::string memberName;

	// Annotations
	MemberVarSymbol* symbol;
	ConstructorSymbol* constructorSymbol;
	size_t memberIndex;
	std::map<TypeVariable*, Type*> typeAssignment;
};


#endif
