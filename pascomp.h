/********************************************************************************************//**
 * @file pascomp.h
 *
 * The Pascal-lite compilier.
 *
 * @author Randy Merkel, Slowly but Surly Software.
 * @copyright  (c) 2017 Slowly but Surly Software. All rights reserved.
 ************************************************************************************************/

#ifndef	COMP_H
#define	COMP_H

#include "compilier.h"

/********************************************************************************************//**
 * A Pascal-lite Compilier
 *
 * Productions for the Pascal-lite recursive decent compilier.
 *
 * Evolved from https://en.wikipedia.org/wiki/Recursive_descent_parser#C_implementation.
 * Construction binds a program name with the instance, used in error messages. The compilier is
 * run via the call operator, specifing the input stream, the location of the emitted code, and
 * wheather to emit a travlelog (verbose messages).
 ************************************************************************************************/
class PasComp : public Compilier {
public:
	PasComp(const std::string& pName);		///< Constructor; use pName for error messages

private:
	bool isAnInteger(TDescPtr type);		///< Is type an integer?
	bool isAReal(TDescPtr type);			///< Is type a Real?

	/// Promote data type if necessary...
	TDescPtr promote(TDescPtr lhs, TDescPtr rhs);

	/// Promote assigned data type if necessary...
	void assignPromote (TDescPtr lhs, TDescPtr rhs);

	/// variable sub-production...
	TDescPtr variable(int level, SymbolTable::iterator it);

	TDescPtr builtInFunc(int level);		///< built-in functions

	/// factor-identifier sub-production...
	TDescPtr identFactor(int level, const std::string& id);

	TDescPtr factor(int level);				///< factor production...
	TDescPtr term(int level);				///< terminal production...
	TDescPtr unary(int level);				///< unary-expr production...
	TDescPtr simpleExpr(int level);			///< simple-expr production...
	TDescPtr expression(int level);			///< expression production...
	TDescPtrVec	expressionList(int level);	///< expression-list production...
	std::pair<bool,Datum> constExpr();		///< const-expr production...

	/// call-statement production...
	void callstatement(int level, SymbolTable::iterator it);

	void whileStatement(int level);			///< while-statement production...
	void ifStatement(int level);			///< if-statement production...
	void repeateStatement(int level);		///< repeat-statement production...
 	void forStatement(int level);			///< for-statement production...

	/// assignment-statement production...
	void assignStatement(int level, SymbolTable::iterator it, bool dup = false);

	/// identifier-statement production...
	void identStatement(int level, const std::string& id);

	void statement(int level);				///< statement production...
	void statementList(int level);			///< statement-list-production...
	void constDeclList(int level);			///< const-declaration-list production...
	void constDecl(int level);				///< constant-declaration production...

	void typeDecl(int level);				///< type-declaracton production...
	void typeDeclList(int level);			///< type-declaraction-list production...

	int varDeclBlock(int level);			///< variable-declaration-block production...
											/// variable-declaration-list production...
	void varDeclList(int level, bool params, NameKindVec& idents);

	/// variable-declaration production...
	void varDecl(int level, NameKindVec& idents);

	/// identifier-lst production...
	std::vector<std::string> identifierList(int level);

	TDescPtr type(int level);				///< type productions...
	TDescPtr simpleType(int level);			///< simple-type productions...
	TDescPtr ordinalType(int level);		///< ordinal type productions...
	TDescPtr structuredType(int level);		///< structured-type productions...

	/// field-list productions...
	void fieldList(int level, NameKindVec& fields);

	TDescPtrVec simpleTypeList(int level);	///< simple-type-list productions...

	/// Subroutine-declaration production...
	SymValue& subPrefixDecl(int level, SymValue::Kind kind);
	void procDecl(int level);				///< procedure-declaration production...
	void funcDecl(int level);				///< function-declaration production...
	void subDeclList(int level);			///< function/procedue declaraction productions...

	/// block-declaration production...
	unsigned blockDecl(SymValue& val, int level);

	void run() override;					///< run the compilier...
};

#endif
