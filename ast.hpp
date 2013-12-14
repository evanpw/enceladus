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

	YYLTYPE* location() { return location_; }
	const std::shared_ptr<Type>& type() { return type_; }

	void setType(const std::shared_ptr<Type>& type) { type_ = type; }

protected:
	YYLTYPE* location_;
	std::shared_ptr<Type> type_;
};

class StatementNode : public AstNode {};
class ExpressionNode : public StatementNode {};



////// Utility classes other than AST nodes ////////////////////////////////////
typedef std::vector<std::unique_ptr<ExpressionNode>> ArgList;
typedef std::vector<std::unique_ptr<TypeName>> TypeDecl;

class ConstructorSpec
{
public:
	ConstructorSpec(const char* name)
	: name_(name)
	{}

	void append(TypeName* typeName) { members_.emplace_back(typeName); }

	const std::string& name() const { return name_; }
	const std::vector<std::unique_ptr<TypeName>>& members() const { return members_; }

	void setMemberTypes(const std::vector<std::shared_ptr<Type>> types)
	{
		assert(types.size() == members_.size());
		types_ = types;
	}
	const std::vector<std::shared_ptr<Type>>& memberTypes() { return types_; }

private:
	std::string name_;

	std::vector<std::unique_ptr<TypeName>> members_;
	std::vector<std::shared_ptr<Type>> types_;
};


////// Top-level nodes /////////////////////////////////////////////////////////

class ProgramNode : public AstNode
{
public:
	ProgramNode()
	: scope_(new Scope)
	, typeTable_(new TypeTable)
	{}

	void append(AstNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::list<std::unique_ptr<AstNode>>& children() const { return children_; }
	Scope* scope() { return scope_.get(); }
	TypeTable* typeTable() { return typeTable_.get(); }

private:
	std::list<std::unique_ptr<AstNode>> children_;
	std::unique_ptr<Scope> scope_;
	std::unique_ptr<TypeTable> typeTable_;
};

////// Expression nodes ////////////////////////////////////////////////////////
class LogicalNode : public ExpressionNode
{
public:
	enum Operator {kAnd, kOr};

	LogicalNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs_(lhs), op_(op), rhs_(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* lhs() { return lhs_.get(); }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_.get(); }

protected:
	std::unique_ptr<ExpressionNode> lhs_;
	Operator op_;
	std::unique_ptr<ExpressionNode> rhs_;
};

class ComparisonNode : public ExpressionNode
{
public:
	enum Operator { kGreater, kEqual, kLess, kGreaterOrEqual, kLessOrEqual, kNotEqual };

	ComparisonNode(ExpressionNode* lhs, Operator op, ExpressionNode* rhs)
	: lhs_(lhs), op_(op), rhs_(rhs)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* lhs() { return lhs_.get(); }
	Operator op() { return op_; }
	ExpressionNode* rhs() { return rhs_.get(); }

protected:
	std::unique_ptr<ExpressionNode> lhs_;
	Operator op_;
	std::unique_ptr<ExpressionNode> rhs_;
};

class NullaryNode : public ExpressionNode
{
public:
	NullaryNode(const char* name) : name_(name), symbol_(nullptr) {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& name() { return name_; }
	const Symbol* symbol() { return symbol_; }

	void attachSymbol(Symbol* symbol) { symbol_ = symbol; }

private:
	std::string name_;
	Symbol* symbol_;
};

class IntNode : public ExpressionNode
{
public:
	IntNode(long value) : value_(value) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	long value() { return value_; }

private:
	long value_;
};

class BoolNode : public ExpressionNode
{
public:
	BoolNode(bool value) : value_(value) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	bool value() { return value_; }

private:
	bool value_;
};

class FunctionCallNode : public ExpressionNode
{
public:
	FunctionCallNode(const char* target, ArgList* arguments)
	: target_(target), symbol_(nullptr)
	{
		arguments_.reset(arguments);
	}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& target() { return target_; }
	ArgList& arguments() { return *arguments_.get(); }

	FunctionSymbol* symbol() { return symbol_; }
	void attachSymbol(FunctionSymbol* symbol) { symbol_ = symbol; }

private:
	std::string target_;
	std::unique_ptr<ArgList> arguments_;

	FunctionSymbol* symbol_;
};



////// Statement nodes /////////////////////////////////////////////////////////
class BlockNode : public StatementNode
{
public:
	void append(StatementNode* child);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::list<std::unique_ptr<StatementNode>>& children() const { return children_; }

private:
	std::list<std::unique_ptr<StatementNode>> children_;
};

class IfNode : public StatementNode
{
public:
	IfNode(ExpressionNode* condition, StatementNode* body) : condition_(condition), body_(body) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_.get(); }
	StatementNode* body() { return body_.get(); }

private:
	std::unique_ptr<ExpressionNode> condition_;
	std::unique_ptr<StatementNode> body_;
};

class IfElseNode : public StatementNode
{
public:
	IfElseNode(ExpressionNode* condition, StatementNode* body, StatementNode* else_body)
	: condition_(condition), body_(body), else_body_(else_body)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_.get(); }
	StatementNode* body() { return body_.get(); }
	StatementNode* else_body() { return else_body_.get(); }

private:
	std::unique_ptr<ExpressionNode> condition_;
	std::unique_ptr<StatementNode> body_;
	std::unique_ptr<StatementNode> else_body_;
};

class WhileNode : public StatementNode
{
public:
	WhileNode(ExpressionNode* condition, StatementNode* body) : condition_(condition), body_(body) {}
	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* condition() { return condition_.get(); }
	StatementNode* body() { return body_.get(); }

private:
	std::unique_ptr<ExpressionNode> condition_;
	std::unique_ptr<StatementNode> body_;
};

// For loops are implemented as pure syntactic sugar
BlockNode* makeForNode(const char* loopVar, ExpressionNode* list, StatementNode* body);

class AssignNode : public StatementNode
{
public:
	AssignNode(const char* target, ExpressionNode* value)
	: target_(target), value_(value), symbol_(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& target() { return target_; }
	ExpressionNode* value() { return value_.get(); }

	VariableSymbol* symbol() { return symbol_; }
	void attachSymbol(VariableSymbol* symbol) { symbol_ = symbol; }

private:
	std::string target_;
	std::unique_ptr<ExpressionNode> value_;

	VariableSymbol* symbol_;
};

class LetNode : public StatementNode
{
public:
	LetNode(const char* target, TypeName* typeName, ExpressionNode* value)
	: target_(target), typeName_(typeName), value_(value), symbol_(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& target() { return target_; }
	TypeName* typeName() { return typeName_.get(); }
	ExpressionNode* value() { return value_.get(); }

	VariableSymbol* symbol() { return symbol_; }
	void attachSymbol(VariableSymbol* symbol) { symbol_ = symbol; }

private:
	std::string target_;
	std::unique_ptr<TypeName> typeName_;
	std::unique_ptr<ExpressionNode> value_;

	VariableSymbol* symbol_;
};

class ParamListNode : public AstNode
{
public:
	void append(const char* param);

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::vector<std::string>& names() const { return names_; }

private:
	std::vector<std::string> names_;
};

class FunctionDefNode : public StatementNode
{
public:
	FunctionDefNode(const char* name, StatementNode* body, ParamListNode* params, TypeDecl* typeDecl)
	: name_(name), body_(body), params_(params), typeDecl_(typeDecl), symbol_(nullptr), scope_(new Scope)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& name() { return name_; }
	StatementNode* body() { return body_.get(); }
	const std::vector<std::string>& params() { return params_->names(); }
	TypeDecl* typeDecl() { return typeDecl_.get(); }

	Scope* scope() { return scope_.get(); }

	FunctionSymbol* symbol() { return symbol_; }
	void attachSymbol(FunctionSymbol* symbol) { symbol_ = symbol; }

private:
	std::string name_;
	std::unique_ptr<StatementNode> body_;
	std::unique_ptr<ParamListNode> params_;
	std::unique_ptr<TypeDecl> typeDecl_;

	FunctionSymbol* symbol_;
	std::unique_ptr<Scope> scope_;
};

class MatchNode : public StatementNode
{
public:
	MatchNode(const char* constructor, ParamListNode* params, ExpressionNode* body)
	: constructor_(constructor), params_(params), body_(body), constructorSymbol_(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& constructor() { return constructor_; }
	ParamListNode* params() { return params_.get(); }
	ExpressionNode* body() { return body_.get(); }

	const std::vector<VariableSymbol*>& symbols() { return symbols_; }
	void attachSymbol(VariableSymbol* symbol) { symbols_.push_back(symbol); }

	FunctionSymbol* constructorSymbol() { return constructorSymbol_; }
	void attachConstructorSymbol(FunctionSymbol* symbol) { constructorSymbol_ = symbol; }

private:
	std::string constructor_;
	std::unique_ptr<ParamListNode> params_;
	std::unique_ptr<ExpressionNode> body_;

	std::vector<VariableSymbol*> symbols_;
	FunctionSymbol* constructorSymbol_;

};

class DataDeclaration : public StatementNode
{
public:
	DataDeclaration(const char* name, ConstructorSpec* constructor)
	: name_(name), constructor_(constructor)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& name() { return name_; }
	ConstructorSpec* constructor() { return constructor_.get(); }

	void attachConstructor(ValueConstructor* valueConstructor) { valueConstructor_ = valueConstructor; }
	ValueConstructor* valueConstructor() { return valueConstructor_; }

private:
	std::string name_;
	std::unique_ptr<ConstructorSpec> constructor_;
	ValueConstructor* valueConstructor_;
};

class ForeignDeclNode : public StatementNode
{
public:
	ForeignDeclNode(const char* name, ParamListNode* params, TypeDecl* typeDecl)
	: name_(name), params_(params), typeDecl_(typeDecl), symbol_(nullptr)
	{}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	const std::string& name() { return name_; }
	const std::vector<std::string>& params() { return params_->names(); }
	TypeDecl* typeDecl() { return typeDecl_.get(); }

	FunctionSymbol* symbol() { return symbol_; }
	void attachSymbol(FunctionSymbol* symbol) { symbol_ = symbol; }

	std::vector<const Type*> paramTypes() const { return paramTypes_; }
	void setParamTypes(const std::vector<const Type*>& paramTypes) { paramTypes_ = paramTypes; }

private:
	std::string name_;
	std::unique_ptr<ParamListNode> params_;
	std::unique_ptr<TypeDecl> typeDecl_;

	FunctionSymbol* symbol_;

	std::vector<const Type*> paramTypes_;
};

class ReturnNode : public StatementNode
{
public:
	ReturnNode(ExpressionNode* expression) : expression_(expression) {}

	virtual void accept(AstVisitor* visitor) { visitor->visit(this); }

	ExpressionNode* expression() { return expression_.get(); }

private:
	std::unique_ptr<ExpressionNode> expression_;
};

#endif
