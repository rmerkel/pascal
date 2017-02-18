/** @file pl0com.cc
 *  
 * A PL0 Compilier
 */

#include "pl0com.h"
#include "pl0int.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <fstream>

using namespace std;
using namespace pl0;

// protected:

/** Write a error message
 *
 * Writes a diagnostic onto standard error output, incrementing the error count. 
 *
 * @param s The error message
 */
void PL0Comp::error(const string& s) {
	cerr << progName << ": " << s << " near line " << ts.lineNum << endl;
	++nErrors;
}

/** Write a error message
 *
 * Writes a diagnostic, in the form "s 't'", onto standard error output, incrementing the error
 * count.
 *
 * @param s The error message
 * @param t Parameter to s.
 */
void PL0Comp::error(const string& s, const string& t) {
	error (s + " \'" + t + "\'");
}

/** Read and return the next token from the token stream
 *  
 * @return The next token from the token stream 
 */ 
Token PL0Comp::next() {
	Token t = ts.get();

	if (verbose)
		cout << progName << ": getting '" << Token::String(t.kind) << "', " << t.string_value << ", " << t.number_value << "\n";

	return t;
}

/** Emit an instruction
 *
 *	Appends (op, level, addr) onto code, and returns it's address.
 *
 *	@param	op		The pl0 instruction operation code
 *	@param	level	The pl0 instruction block level value. Defaults to zero.
 *	@param	addr	The pl0 instructions address/value. Defautls to zero.
 *
 *	@return The address (code[] index) of the new instruciton.
 */
size_t PL0Comp::emit(const OpCode op, int8_t level, Word addr) {
	if (verbose)
		cout << progName << ": emitting " << code.size() << ": "
				<< toString(op) << " " << static_cast<unsigned>(level) << ", " << addr
				<< "\n";

	code.push_back({op, level, addr});
	return code.size() - 1;				// so it's the address of just emitted instruction
}

/** Accept the next token
 *
 * Returns true, and optionally consumes the current token, if it's type is euqal to kind.
 *
 * @param	kind	The token Kind to accept.
 * @param	get		Consume the token, if true. Defaults true.
 *
 * @return	true if the current token is a kind.
 */
bool PL0Comp::accept(Token::Kind kind, bool get) {
	if (current() == kind) {
		if (get) next();			// consume the token
		return true;
	}

	return false;
}

/** Expect the next token
 *
 * Evaluate and return accept(kind, get). Generate an error if accept() returns false.
 *
 * @param	kind	The token Kind to accept.
 * @param	get		Consume the token, if true. Defaults true.
 *
 * @return	true if the current token is a k.
 */
bool PL0Comp::expect(Token::Kind kind, bool get) {
	if (accept(kind, get)) return true;

	error("expected", Token::String(kind) + "\' got \'" + Token::String(current()));
	return false;
}

/** Factor identifier
 *
 * Push a variable or a constant value.
 *
 * @param	level	The current block level
 */
void PL0Comp::identifier(unsigned level) {
	const string name = ts.current().string_value;

	if (expect(Token::identifier)) {
		auto range = symtbl.equal_range(name);
		if (range.first == range.second)
			error("Undefined identifier", name);

		auto closest = range.first;
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level > closest->second.level)
				closest = it;
		if (closest->second.kind == SymValue::constant)
			emit(OpCode::pushConst, 0, closest->second.value);
		else									// mutable variable
			emit(OpCode::pushVar, level - closest->second.level, closest->second.value);
	}
}

/** Factor
 *
 * factor = ident | number | "{" expression "}" ;
 *
 * @param	level	The current block level.
 */
void PL0Comp::factor(unsigned level) {
	if (accept(Token::identifier, false))
		identifier(level);

	else if (accept(Token::number, false)) {
		emit(OpCode::pushConst, 0, ts.current().number_value);
		expect(Token::number);

	} else if (accept(Token::lparen)) {
		expression(level);
		expect(Token::rparen);

	} else {
		error("factor: syntax error; expected ident | num | { expr }, but got:",
			Token::String(current()));
		next();
	}
}

/** Terminal
 *
 * term = fact { (*|/) fact } ;
 *
 * @param	level	The current block level.
 */
void PL0Comp::terminal(unsigned level) {
	factor(level);

	for (Token::Kind k = current(); k == Token::mul || k == Token::div; k = current()) {
		next();							// consume the token

		factor(level);
		Token::mul == k ? emit(OpCode::mul) : emit(OpCode::div);
	}
}

/** Expression
 *
 * expr = [ +|-] term { (+|-) term } ;
 *
 * @param	level	The current block level.
 */
void PL0Comp::expression(unsigned level) {
	const Token::Kind unary = current();	// unary [ +|-]?
	if (unary == Token::add || unary == Token::sub)
		next();

	terminal(level);
	if (Token::sub == unary)
		emit(OpCode::neg);				// ignore unary +!

	for (Token::Kind k = current(); k == Token::add || k == Token::sub; k = current()) {
		next();							// consume the token

		terminal(level);
		Token::add == k ? emit(OpCode::add) : emit(OpCode::sub);
	}
}

/** Condition
 *
 * cond =  "odd" expr | expr ("="|"!="|"<"|"<="|">"|">=") expr ;
 *
 * @param	level	The current block level.
 */
void PL0Comp::condition(unsigned level) {
	if (accept(Token::odd)) {
		expression(level);
		emit(OpCode::odd);

	} else {
		expression(level);
		const Token::Kind op = current();

		if (accept(Token::lte)		||
			accept(Token::lt)		||
			accept(Token::equ)		||
			accept(Token::gt)		||
			accept(Token::gte)		||
			accept(Token::neq))
		{
			expression(level);

			switch(op) {
			case Token::lte:		emit(OpCode::lte);	break;
			case Token::lt:			emit(OpCode::lt);	break;
			case Token::equ:		emit(OpCode::equ);	break;
			case Token::gt:			emit(OpCode::gt);	break;
			case Token::gte:		emit(OpCode::gte);	break;
			case Token::neq:		emit(OpCode::neq);	break;
			default:			assert(false);		// can't get here!
			}
		}
	}
}

/** Assignment statement
 *
 * ident "=" expression
 *
 * @param	level	The current block level.
 */
void PL0Comp::assignStmt(unsigned level) {
	// Save the identifier string and loop it up in the symbol table, before consuming it
	const string name = ts.current().string_value;
	next();												// Consume the identifier

	auto range = symtbl.equal_range(name);
	if (range.first == range.second)
		error("undefined variable", name);

	expect(Token::assign);								// Consume "="
	expression(level);									// Process he expression

	if (range.first != range.second) {
		auto closest = range.first;						// Find the closest identifier
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level > closest->second.level)
				closest = it;

		if (SymValue::identifier != closest->second.kind)
			error("identifier is not mutable", name);
		else
			emit(OpCode::pop, level - closest->second.level, closest->second.value);
	}
}

/** Call statement
 *  
 *  "call" identifier...
 *  
 * @param	level	The current block level
 */ 
void PL0Comp::callStmt(unsigned level) {
	const string name = ts.current().string_value;

	expect(Token::identifier);
	auto range = symtbl.equal_range(name);
	if (range.first == range.second)
		error("undefiend identifier", name);

	else {
		auto closest = range.first;
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level > closest->second.level)
				closest = it;
			if (SymValue::proc != closest->second.kind)
				error("Identifier is not a prodedure", name);
			else
				emit(OpCode::call, level - closest->second.level, closest->second.value);
	}
}

/** While statement
 *
 * "while" condition "do" statement...
 *
 * @param	level	The current block level.
 */
 void PL0Comp::whileStmt(unsigned level) {
	const size_t cond_pc = code.size();				// Start of while condition
	condition(level);

	const size_t jmp_pc = emit(OpCode::jneq, 0, 0);	// jump if condition false
	expect(Token::Do);								// consume "do"
	statement(level);

	emit(OpCode::jump, 0, cond_pc);					// Jump back to conditon test

	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << code.size() << "\n";
	code[jmp_pc].addr = code.size();				// Patch jump on condition false instruction
 }

/** if statement
 *  
 *  "if" conditon "then" statement1 [ "else" statement2 ]
 */
 void PL0Comp::ifStmt(unsigned level) {
	condition(level);

	const size_t jmp_pc = emit(OpCode::jneq, 0, 0);	// jump if condition false
	expect(Token::then);							// Consume "then"
	statement(level);

	// Jump over else statement, but only if there is an else
	const bool Else = accept(Token::Else);
	size_t else_pc = 0;
	if (Else) else_pc = emit(OpCode::jump, 0, 0);
	
	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << code.size() << "\n";
	code[jmp_pc].addr = code.size();				// Patch jump on condition false instruction

	if (Else) {
		statement(level);

		if (verbose)
			cout << progName << ": patching address at " << else_pc << " to " << code.size() << "\n";
		code[else_pc].addr = code.size();				// Patch jump on condition false instruction
	}
 }

 /** Repeat statement
  *  
  * [ "repeat" stmt "until" cond ] 
  *  
  * @param	level 	The current block level 
  */
 void PL0Comp::repeatStmt(unsigned level) {
	 const size_t loop_pc = code.size();			// jump here until condition fails
	 statement(level);
	 expect(Token::until);
	 condition(level);
	 emit(OpCode::jneq, 0, loop_pc);
 }

/** Statement
 *
 * stmt =	[ ident ":=" expr
 * 		  	| "call" ident
 *          | "?" ident
 * 		  	| "!" expr
 *          | "begin" stmt {";" stmt } "end"
 *          | "if" cond "then" stmt { "else" stmt }
 *          | "while" cond "do" stmt ] ;
 *
 * @param	level	The current block level.
 */
void PL0Comp::statement(unsigned level) {
	if (accept(Token::identifier, false)) 			// assignment
		assignStmt(level);

	else if (accept(Token::call))					// procedure call
		callStmt(level);

	else if (accept(Token::begin)) {				// begin ... end
		do {
			statement(level);
		} while (accept(Token::scomma));
		expect(Token::end);

	} else if (accept(Token::If)) 					// if condition...
		ifStmt(level);

	else if (accept(Token::While))					// "while" condition...
		whileStmt(level);

	else if (accept(Token::repeat))
		repeatStmt(level);

	// else: nothing
}

/** Constant declaration
 *
 * [ const ident = number {, ident = number} ; ]
 *
 * @note Doesn't emit any code; just stores the named value in the symbol table.
 *
 * @param	level	The current block level.
 */
void PL0Comp::constDecl(unsigned level) {
	// Capture the identifier name before consuming it...
	const string name = ts.current().string_value;

	expect(Token::identifier);						// Consume the identifier
	expect(Token::assign);							// Consume the "="
	if (expect(Token::number, false)) {
		auto number = ts.current().number_value;
		next();										// Consume the number

		auto range = symtbl.equal_range(name);
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level == level) {
				error("identifier has previously been defined", name);
				return;
			}

		symtbl.insert({ name, { SymValue::constant, level, number } });
		if (verbose) 
			cout << progName << ": constDecl " << name << ": " << level << ", " << number << "\n";
	}
}


/** Variable declaration
 *
 * Allocate space on the stack for the variable and install it's offset from the block in the symbol
 * table.
 *  
 * @param	offset	Stack offset for the next varaible
 * @param	level	The current block level.
 * 
 * @return	Stack offset for the next varaible.
 */
int PL0Comp::varDecl(int offset, unsigned level) {
	const string name = ts.current().string_value;

	if (expect(Token::identifier)) {
		auto range = symtbl.equal_range(name);
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level == level) {
				error("identifer has previously been defined", name);
				return offset;
			}

		symtbl.insert({ name, { SymValue::identifier, level, offset } });
		if (verbose) 
			cout << progName << ": varDecl " << name << ": " << level << ", " << offset << "\n";
		return offset + 1;
	}

	return offset;
}

/** Procedure declaration
 *
 * { "procedure" ident ";" block ";" }
 *
 * @param	level	The current block level.
 */
void PL0Comp::procDecl(unsigned level) {
	const string name = ts.current().string_value;

	if (expect(Token::identifier)) {
		auto range = symtbl.equal_range(name);
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level == level)
				error("identifier has previously been defined", name);
		
		auto it = symtbl.insert({ name, { SymValue::proc, level, 0 } });
		if (verbose) 
			cout << progName << ": procDecl " << name << ": " << level << ", 0\n";

		expect(Token::scomma);	// procedure name ";"
		block(it, level+1);
		expect(Token::scomma);	// procedure declarations end with a ';'!
	}
}

/** program block
 *
 * block = 	[ const ident = number {, ident = number} ;]
 *         	[ var ident {, ident} ;]
 *         	{ procedure ident ; block ; }
 *          stmt ;
 *  
 * @param	it	The blocks (procedures) symbol table entry
 * @param	level	The current block level.
 */
void PL0Comp::block(SymbolTable::iterator it, unsigned level) {
	auto 	jmp_pc	= emit(OpCode::jump, 0, 0);		// Addr to be patched below..
	int		dx		= 3;							// Variable offset from block/frame

	if (accept(Token::constDecl)) {					// const ident = number, ...
		do {
			constDecl(level);

		} while (accept(Token::comma));
		expect(Token::scomma);
	}

	if (accept(Token::varDecl)) {					// var ident, ...
		do {
			dx = varDecl(dx, level);

		} while (accept(Token::comma));
		expect(Token::scomma);
	}

	while (accept(Token::procDecl))					// procedure ident; ...
		procDecl(level);

	/*
	 * Block body
	 *
	 * Emit the block prefix, and then set the blocks staring address, and
	 * patch the jump to it
	 */

	auto addr = emit(OpCode::enter, 0, dx);
	if (verbose) cout << progName << ": patching address at " << jmp_pc << " to " << addr  << "\n";
	code[jmp_pc].addr = it->second.value = addr;
	statement(level);								// block body...
	emit(OpCode::ret);								// block postfix

	// Finally, remove symbols only visable to this level
	for (auto i = symtbl.begin(); i != symtbl.end(); )
		if (i->second.level == level) {
			if (verbose) cout << ": purging " << i->first << " from the symbol table\n";
			i = symtbl.erase(i);

		} else
			++i;
}

//public:

/// Constructor; construct a compilier; use pName for errors
PL0Comp::PL0Comp(const string& pName) : progName {pName}, nErrors{0}, verbose {false}, ts{cin} {
	symtbl.insert({"main", { SymValue::proc, 0, 0 }});	// Install the "main" rountine declaraction
}

/// Compile...
void PL0Comp::run() {
	next();
	auto range = symtbl.equal_range("main");
	assert(range.first != range.second);

	block(range.first, 0);
	expect(Token::period);
}

/** Run the compilier
 *
 * @param	inFile	The source file name
 * @param	prog	The generated machine code is appended here
 * @param	v		Output verbose messages if true
 *
 * @return	The number of errors encountered
 */
unsigned PL0Comp::operator()(const string& inFile, InstrVector& prog, bool v) {
	verbose = v;

	if ("-" == inFile)  {							// "-" means standard input
		ts.set_input(cin);
		run();

	} else {
		ifstream ifile(inFile);
		if (!ifile.is_open())
			error("error opening soruce file", inFile);

		else {
			ts.set_input(ifile);
			run();
		}
	}

	if (verbose) {									// disassemble results and dump the symbol table
		cout << "\n";
		unsigned loc = 0;
		for (auto it : code)
			disasm(loc++, it);
		cout << endl;
	}

	prog = code;									// return the results
		
	return nErrors;
}
