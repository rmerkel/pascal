/** @file pl0com.h
 *
 * A PL/0 compiler...
 *
 * Grammar (EBNF)
 * --------------
 * prog =	block "." ;
 * 
 * block = 	[ "const" ident "=" number {"," ident "=" number} ";"]
 *         	[ "var" ident {"," ident} ";"]
 *         	{ "procedure" ident ";" block ";" }
 *          stmt ;
 * 
 * stmt = 	[ ident ":=" expr
 * 		  	| "call" ident 
 *          | "?" ident 
 * 		  	| "!" expr 
 *          | "begin" stmt {";" stmt } "end" 
 *          | "if" cond "then" stmt { "else" stmt }
 *  		| "while" cond "do" stmt ]
 *  		| "repeat" stmt "until" cond ;
 * 
 * cond = 	  "odd" expr
 * 		  	| expr ("="|"!="|"<"|"<="|">"|">=") expr ;
 * 
 * expr = 	[ "+"|"-"] term { ("+"|"-") term } ;
 * 
 * term = 	fact {("*"|"/") fact} ;
 * 
 * fact = 	  ident
 *			| number
 *			| "(" expr ")" ;
 * 
 * Key
 * 	- {}	zero or more times
 * 	- []	zero or one times
 */

#ifndef	PL0COM_H
#define	PL0COM_H

#include "pl0.h"
#include "token.h"
#include "symbol.h"

#include <string>

/// A PL0 Compiler
class PL0Comp {
	std::string			progName;		///< The owning programs name
	unsigned			nErrors;		///< # of errors compiling all sources
	bool				verbose;		///< Dump debugging information if true
	pl0::InstrVector 	code;			///< Emitted code
	TokenStream			ts;				///< Input token stream
	SymbolTable			symtbl;			///< The symbol table

protected:
	void error(const std::string& s);
	void error(const std::string& s, const std::string& t);
	Token next();

	/// Return the current token kind
	Token::Kind current() 				{	return ts.current().kind;	}

	size_t emit(const pl0::OpCode op, int8_t level = 0, pl0::Word addr = 0);
	bool accept(Token::Kind k, bool get = true);
	bool expect(Token::Kind k, bool get = true);
	void identifier(unsigned level);
	void factor(unsigned level);
	void terminal(unsigned level);
	void expression(unsigned level);
	void condition(unsigned level);
	void assignStmt(unsigned level);
	void callStmt(unsigned level);
	void whileStmt(unsigned level);
	void repeatStmt(unsigned level);
 	void ifStmt(unsigned level);
	void statement(unsigned level);
	void constDecl(unsigned level);
	int varDecl(int offset, unsigned level);
	void procDecl(unsigned level);
	void block(SymbolTable::iterator it, unsigned level);
	void run();

public:
	PL0Comp(const std::string& pName);
	virtual ~PL0Comp() {}

	unsigned operator()(const std::string& inFile, pl0::InstrVector& code, bool verbose = false);
};

#endif
