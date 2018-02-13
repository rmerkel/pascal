/********************************************************************************************//**
 * @file pcomp.cc
 *
 * P Language Compiler implementation
 *
 * @author Randy Merkel, Slowly but Surly Software.
 * @copyright  (c) 2017 Slowly but Surly Software. All rights reserved.
 ************************************************************************************************/

#include "pcomp.h"
#include "pinterp.h"

#include <cassert>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;
using namespace rel_ops;

/************************************************************************************************
 * class PComp
 ************************************************************************************************/

// private:

/********************************************************************************************//**
 * @param	type	Type descriptor to investagate
 * @return  true if type can be treated as an Integer
 ************************************************************************************************/
bool PComp::isAnInteger(TDescPtr type) {
	return type->tclass() == TypeDesc::Integer;
}

/********************************************************************************************//**
 * @param	type	Type descriptor to investagate
 * @return  true if type can be treated as an Real
 ************************************************************************************************/
bool PComp::isAReal(TDescPtr type) {
	return	type->tclass() == TypeDesc::Real;
}

/********************************************************************************************//**
 * Promote binary stack operands as necessary. 
 *
 * @param	lhs	The type of the left-hand-side
 * @param	rhs	The type of the right-hand-side 
 *
 * @return	Type type of the promoted type
 ************************************************************************************************/
TDescPtr PComp::promote(TDescPtr lhs, TDescPtr rhs) {
	TDescPtr type = lhs;					// Assume that lhs and rhs have the same types
	if (lhs->tclass() == rhs->tclass())
		;									// nothing to do

	else if ((isAnInteger(lhs) && isAnInteger(rhs)) || (isAReal(lhs) && isAReal(rhs)))
		;									// nothing to do, again

	else if (isAnInteger(lhs) && isAReal(rhs)) {
		emit(OpCode::ITOR2);				// promote lhs (TOS-1) to a real
		type = rhs;

	} else if (isAReal(lhs) && isAnInteger(rhs))
		emit(OpCode::ITOR);					// promote rhs to a real
	
	else
		error("incompatable binary types");

	return type;
}

/********************************************************************************************//**
 * Convert rhs of assign to real if necessary, or emit error would need to be
 * converted to an integer. 
 *
 * @param	lhs	The type of the left-hand-side
 * @param	rhs	The type of the right-hand-side 
 ************************************************************************************************/
void PComp::assignPromote (TDescPtr lhs, TDescPtr rhs) {
	if (lhs->tclass() == rhs->tclass())
		;				// nothing to do

	else if ((isAnInteger(lhs) && isAnInteger(rhs)) || (isAReal(lhs) && isAReal(rhs)))
		;				// nothing to do, again

	else if (isAnInteger(lhs) && isAReal(rhs)) {
		error("rounding real to fit in an integer");
		emit(OpCode::ROUND);				// promote rhs to a integer	 

	} else if (isAReal(lhs) && isAnInteger(rhs))
		emit(OpCode::ITOR);                 // promote rhs to a real

	else
		error("incompatable assignment types");

	// Emit limit checks, unless range is impossible to exceed
	if (lhs->isOrdinal() && lhs->range() != TypeDesc::maxRange) {
		emit(OpCode::LLIMIT, 0, lhs->range().minimum());
		emit(OpCode::ULIMIT, 0, lhs->range().maximum());
	}
}

/********************************************************************************************//**
 * Push a variable's value, a constant value, or invoke a function, and push the results,
 * of a function.
 *
 * ident | ident [ ( expr-lst ) ]
 *
 * @param	id		The variable or prodecure identifier
 * @param	level	The current block level 
 * @return	Data type 
 ************************************************************************************************/
TDescPtr PComp::identFactor(int level, const string& id) {
	auto type = TypeDesc::intDesc;
   	auto it = lookup(id);

	if (it != symtbl.end()) {
		switch (it->second.kind()) {
		case SymValue::Constant:
			type = it->second.type();
			emit(OpCode::PUSH, 0, it->second.value());
			break;

		case SymValue::Variable:
			type = variable(level, it);
			emit(OpCode::EVAL, 0, type->size());
			break;

		case SymValue::Function:
			type = it->second.type();		// Use the function return type
			callStatement(level, it);
			break;

		default:
			error("Identifier is not a constant, variable or function", it->first);
		}
	}

	return type;
}

/********************************************************************************************//**
 * @param level		The current block level.
 *
 * @return	Data type 
 ************************************************************************************************/
TDescPtr PComp::builtInFunc(int level)
{
	auto type = TypeDesc::intDesc;			// Factor data type
	ostringstream oss;

	if (accept(Token::Round))  {		// round(expr) to an integer
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() != TypeDesc::Real) {
			oss << "expeced real value, got: " << current();
			error(oss.str());

		} else {
			emit(OpCode::ROUND);
			type = TypeDesc::intDesc;
		}

	} else if (accept(Token::Trunc)) {	// truncate(expr) to an integer
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() != TypeDesc::Real) {
			oss << "expeced real value, got: " << current();
			error(oss.str());

		} else {
			emit(OpCode::TRUNC);
			type = TypeDesc::intDesc;
		}

	} else if (accept(Token::Abs)) {	// replace TOS with it's absolute value
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() != TypeDesc::Integer && type->tclass() != TypeDesc::Real)
			oss << "expeced integer or real value, got: " << current();

		else
			emit(OpCode::ABS);

	} else if (accept(Token::Atan)) {	// replace TOS with it's arc tangent
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() == TypeDesc::Integer)
			type = TypeDesc::realDesc;		// Always produces a real
		else if (type->tclass() != TypeDesc::Real)
			oss << "expeced integer, or real value, got: " << current();
		emit(OpCode::ATAN);

	} else if (accept(Token::Exp)) {	// Replace TOS with e raised to the given power TOS
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() == TypeDesc::Integer)
			type = TypeDesc::realDesc;		// Always produces a real
		else if (type->tclass() != TypeDesc::Real)
			oss << "expeced integer, or real value, got: " << current();
		emit(OpCode::EXP);

	} else if (accept(Token::Log)) {	// Replace TOS with log(TOS)
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() == TypeDesc::Integer)
			type = TypeDesc::realDesc;		// Always produces a real
		else if (type->tclass() != TypeDesc::Real)
			oss << "expeced integer, or real value, got: " << current();
		emit(OpCode::LOG);

	} else if (accept(Token::Odd)) {	// Replace TOS with is TOS odd?
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() != TypeDesc::Integer)
			oss << "expeced integer value, got: " << current();
		emit(OpCode::ODD);

	} else if (accept(Token::Pred)) {	// Replace TOS with its predicessor
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->isOrdinal())
			oss << "expeced ordinal value, got: " << current();
		emit(OpCode::PRED, 0, type->range().minimum());

	} else if (accept(Token::Sin)) {	// Replace TOS with sin(TOS)
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() == TypeDesc::Integer)
			type = TypeDesc::realDesc;		// Always produces a real
		else if (type->tclass() != TypeDesc::Real)
			oss << "expeced integer, or real value, got: " << current();
		emit(OpCode::SIN);

	} else if (accept(Token::Sqr)) {	// Replace TOS with TOS * TOS
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() == TypeDesc::Integer)
			type = TypeDesc::realDesc;		// Always produces a real
		else if (type->tclass() != TypeDesc::Real)
			oss << "expeced integer, or real value, got: " << current();
		emit(OpCode::SQR);

	} else if (accept(Token::Sqrt)) {	// Replace TOS with sqrt(TOS)
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->tclass() == TypeDesc::Integer)
			type = TypeDesc::realDesc;		// Always produces a real
		else if (type->tclass() != TypeDesc::Real)
			oss << "expeced integer, or real value, got: " << current();
		emit(OpCode::SQRT);

	} else if (accept(Token::Succ)) {	// Replace TOS with is TOS odd?
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (type->isOrdinal())
			oss << "expeced ordinal value, got: " << current();
		emit(OpCode::SUCC, 0, type->range().maximum());

	} else if (accept(Token::Ord)) {	// replace ordinal with it's ordinal
		expect(Token::OpenParen);
		type = expression(level);
		expect(Token::CloseParen);	

		if (!type->isOrdinal()) {
			oss << "expected ordinal, got: " << current();
			error(oss.str());
		} else 
			type = TypeDesc::intDesc;

	} else {
		oss << "bultInFunc: syntax error; expected ident | num | { expr }, got: " << current();
		error(oss.str());
		next();
	}
		
	return type;
}

/********************************************************************************************//**
 * ident                                |
 * round ( expr )						|
 * ident [ ( expr-lst ) ]				|
 * number                               |
 * ( expr )
 * 
 * @param level		The current block level.
 *
 * @return	Data type 
 ************************************************************************************************/
TDescPtr PComp::factor(int level) {
	auto type = TypeDesc::intDesc;			// Factor data type

	if (accept(Token::Identifier, false)) {
		// Copy, and then consume the identifer...
    	const string id = ts.current().string_value;
    	next();
		type = identFactor(level, id);

	} else if (accept(Token::IntegerNum, false)) {
		int value = ts.current().integer_value;
		emit(OpCode::PUSH, 0, value);
		expect(Token::IntegerNum);
		type = TypeDesc::intDesc;

	} else if (accept(Token::RealNum, false)) {
		emit(OpCode::PUSH, 0, ts.current().real_value);
		expect(Token::RealNum);
		type = TypeDesc::realDesc;

	} else if (accept(Token::OpenParen)) {
		type = expression(level);
		expect(Token::CloseParen);

	} else if (accept(Token::Not)) {
		emit(OpCode::NOT);
		type = factor(level);

	} else if (accept(Token::String, false)) {	// TBD: type = stringLiteral()
		const auto s = ts.current().string_value;
		next();

		if (s.size() == 1) {
			emit(OpCode::PUSH, 0, s[0]);
			type = TypeDesc::charDesc;

		} else {								// push each character...
			for (char c : s)
				emit(OpCode::PUSH, 0, c);
			type = TypeDesc::newArrayDesc(s.size(), SubRange(0, s.size() - 1), TypeDesc::intDesc, TypeDesc::charDesc);

		}

	} else 
		type = builtInFunc(level);

	return type;
}

/********************************************************************************************//**
 * fact { ( * | / | mod | and ) fact } ;
 *
 * @param level		The current block level 
 *
 * @return	Data type  
 ************************************************************************************************/
TDescPtr PComp::term(int level) {
	auto lhs = factor(level);

	for (;;) {
		if (accept(Token::Multiply)) {
			lhs = promote(lhs, factor(level));
			emit(OpCode::MUL);
			
		} else if (accept(Token::Divide)) {
			lhs = promote(lhs, factor(level));
			emit(OpCode::DIV);	
			
		} else if (accept(Token::Mod)) {
			lhs = promote(lhs, factor(level));
			emit(OpCode::REM);

		} else if (accept(Token::And)) {
			lhs = promote(lhs, factor(level));
			emit(OpCode::AND);

		} else
			break;
	}

	return lhs;
}

/********************************************************************************************//**
 * [ ( + | - ] term
 *
 * @param level		The current block level 
 *
 * @return	Data type 
 ************************************************************************************************/
TDescPtr PComp::unary(int level) {
	auto type = TypeDesc::intDesc;			// Default factor data type

	if (accept(Token::Add)) 
		type = term(level);					// ignore unary + 

	else if (accept(Token::Subtract)) {
		type = term(level);
		emit(OpCode::NEG);

	} else									
		type = term(level);

	return type;
}

/********************************************************************************************//**
 * term { (+ | - | or ) term } ;
 *
 * @param level		The current block level. 
 *
 * @return	Data type  
 ************************************************************************************************/
TDescPtr PComp::simpleExpr(int level) {
	auto lhs = unary(level);

	for (;;) {
		if (accept(Token::Add)) {
			lhs = promote(lhs, unary(level));
			emit(OpCode::ADD);

		} else if (accept(Token::Subtract)) {
			lhs = promote(lhs, unary(level));
			emit(OpCode::SUB);

		} else if (accept(Token::Or)) {
			lhs = promote(lhs, unary(level));
			emit(OpCode::OR);

		} else
			break;
	}

	return lhs;
}

/********************************************************************************************//**
 * simpleExpr { (< | <= | = | >= | > | <> ) simpleExpr } ;
 *
 * @param level		The current block level 
 *
 * @return	Data type. 
 ************************************************************************************************/
TDescPtr PComp::expression(int level) {
	auto lhs = simpleExpr(level);

	for (;;) {
		if (accept(Token::LTE)) {
			lhs = promote(lhs, simpleExpr(level));
			emit(OpCode::LTE);

		} else if (accept(Token::LT)) {
			lhs = promote(lhs, simpleExpr(level));
			emit(OpCode::LT);

		} else if (accept(Token::GT)) {
			lhs = promote(lhs, simpleExpr(level));
			emit(OpCode::GT);
			
		} else if (accept(Token::GTE)) {
			lhs = promote(lhs, simpleExpr(level));
			emit(OpCode::GTE);
			
		} else if (accept(Token::EQU)) {
			lhs = promote(lhs, simpleExpr(level));
			emit(OpCode::EQU);

		} else if (accept(Token::NEQ)) {
			lhs = promote(lhs, simpleExpr(level));
			emit(OpCode::NEQ);

		} else
			break;
	}

	return lhs;
}

/********************************************************************************************//**
 * expr-lst: expression { ',' expression } ;
 *
 * @param level		The current block level 
 *
 * @return a list (vector) of TDescPtrs, one for each array dimension
 ************************************************************************************************/
TDescPtrVec	PComp::expressionList(int level) {
	TDescPtrVec	v;

	do {
		v.push_back(expression(level));
	} while(accept(Token::Comma));

	return v;
}

/********************************************************************************************//**
 * [ + | - ] number | (const) identifer
 *
 * @return A boolean, constant value pair. Second is valid only if first is true.
 ************************************************************************************************/
pair<bool,Datum> PComp::constExpr() {
	auto value = make_pair(true, Datum(0));
	int sign = 1;

	if (accept(Token::Add))
		;											// ignore unary + 
	else if (accept(Token::Subtract))
		sign = -1;

	if (accept(Token::IntegerNum, false)) {
		value.second = sign * ts.current().integer_value;
		next();										// Consume the number

	} else if (accept(Token::RealNum, false)) {
		value.second = sign * ts.current().real_value;
		next();										// Consume the number

	} else if (accept(Token::Identifier, false)) {
		// Copy, and then, consume the identifier..
		const string id = ts.current().string_value;
		expect(Token::Identifier);

		auto it = lookup(id);
		if (it != symtbl.end()) {
			if (it->second.kind() == SymValue::Constant)
				value.second = sign * it->second.value();
			else
				error("Identifier is not a constant, variable or function", it->first);
		}

	} else
		value.first = false;

	return value;
}

/********************************************************************************************//**
 * Call a function or procedure...
 *
 * ident [ '(' expr-list ')' ]...
 *
 * @param	level	The current block level 
 * @param	it		The sub-routines symbol table entry
 ************************************************************************************************/
void PComp::callStatement(int level, SymbolTable::iterator it) {
	if (accept(Token::OpenParen)) {
		unsigned nParams = 0;					// Count actual parameters
		const auto& params = it->second.params();		// Formal parameter kinds
		if (!accept(Token::CloseParen, false))
			do {								// collect actual parameters
				const auto kind = expression(level);
				if (params.size() > nParams)
					assignPromote(params[nParams], kind);
				++nParams;

			} while (accept (Token::Comma));

		expect(Token::CloseParen);

		if (nParams != params.size()) {			// Is the caller passing right # of params?
			ostringstream oss;
			oss << "passing " << nParams << " parameters, where " << params.size() << " expected";
			error(oss.str());
		}
	}

	if (SymValue::Procedure != it->second.kind() && SymValue::Function != it->second.kind())
		error("Identifier is not a function or procedure", it->first);

	emit(OpCode::CALL, level - it->second.level(), it->second.value());
}

/********************************************************************************************//**
 * while expr do statement...
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::whileStatement(int level) {
	const auto cond_pc = code->size();	// Start of while expr
	expression(level);

	// jump if expr is false...
	const auto jmp_pc = emit(OpCode::JNEQ, 0, 0);
	expect(Token::Do);					// consume "do"
	statement(level);

	emit(OpCode::JUMP, 0, cond_pc);		// Jump back to expr test...

	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << code->size() << "\n";
	(*code)[jmp_pc].addr = code->size(); 
}

/********************************************************************************************//**
 * if expr then statement [ else statement ]
 *
 * @param	level 	The current block level
 ************************************************************************************************/
void PComp::ifStatement(int level) {
	expression(level);

	// Jump if conditon is false
	const size_t jmp_pc = emit(OpCode::JNEQ, 0, 0);
	expect(Token::Then);							// Consume "then"
	statement(level);

	// Jump over else statement, but only if there is an else
	const bool Else = accept(Token::Else);
	size_t else_pc = 0;
	if (Else) else_pc = emit(OpCode::JUMP, 0, 0);

	if (verbose)
		cout << progName << ": patching address at " << jmp_pc << " to " << code->size() << "\n";
	(*code)[jmp_pc].addr = code->size();

	if (Else) {
		statement(level);

		if (verbose)
			cout << progName << ": patching address at " << else_pc << " to " << code->size() << "\n";
		(*code)[else_pc].addr = code->size();
	}
}

/********************************************************************************************//**
 * repeat statement until expr
 *
 * @param	level 	The current block level
 ************************************************************************************************/
void PComp::repeateStatement(int level) {
	 const size_t loop_pc = code->size();			// jump here until expr fails
	 statement(level);
	 expect(Token::Until);
	 expression(level);
	 emit(OpCode::JNEQ, 0, loop_pc);
}

/********************************************************************************************//**
 * for identifier := expresson ( to | downto ) condition do statement
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::forStatement(int level) {
	expect(Token::Identifier, false);	// "for identifier := expression..."
	const string id = ts.current().string_value;
	next();
	auto var = lookup(id);
	if (var == symtbl.end())
		return;							// Unidentified
	assignStatement(level, var, true);										//		addr

	int inc;							// ".. ( to | downto ) .."
	if (accept(Token::To))
		inc = 1;						// "to"
	else {
		expect(Token::DownTo);
		inc = -1;						// "downto"
	}

	const auto cond_pc = code->size();	// Check condition
	emit(OpCode::DUP);					// Duplicate variable refernece				addr, addr
	emit(OpCode::EVAL, 0, 1);			// 											addr, value
	expression(level);					// 											addr, value, cond
	emit(OpCode::LTE);					// 											addr, cond?

	// jump if expr is false...
	const auto jmp_pc = emit(OpCode::JNEQ, 0, 0);								//	addr

	expect(Token::Do);					// "do statement"
	statement(level);

	emit(OpCode::DUP);					// iterate 									addr, addr
	emit(OpCode::DUP);															//	addr, addr, addr
	emit(OpCode::EVAL, 0, 1);													//	addr, addr, value
	emit(OpCode::PUSH, 0, inc);													//	addr, addr, value, 1
	emit(OpCode::ADD);															//	addr, addr, new_value
	emit(OpCode::ASSIGN, 0, 1);													//	addr

	emit(OpCode::JUMP, 0, cond_pc);

	const auto pop_pc = emit(OpCode::POP, 0, 1);

	if (verbose)
		cout << progName << ": patching address at " << pop_pc << " to " << code->size() << "\n";
	(*code)[jmp_pc].addr = pop_pc;
}

/********************************************************************************************//**
 * statement { ; statement }
 *
 * @param	level		The current block level.
 ************************************************************************************************/
void PComp::statementList(int level) {
	do {
		statement(level);
	} while (accept(Token::SemiColon));
}

/********************************************************************************************//**
 * Array index expression-lst.
 *
 * Process a possibly multi-dimensional,  array index. The opening bracket has already been
 * consumed, and the caller will consume consume the closing bracket.
 *
 * @param	level	The current block level.
 * @param	it		The arrays's entry into the symbol table
 * @param	type	The array's type
 *
 * @return	The arrays base type
 ************************************************************************************************/
TDescPtr PComp::varArray(int level, SymbolTable::iterator it, TDescPtr type) {
	TDescPtr atype = type;					// The arrays type, e.g, ArrayDesc
	type = atype->base();					// We'll return the arrays base type...

	if (atype->tclass() != TypeDesc::Array)
		error("attempt to index into non-array", it->first);

	auto indexes = expressionList(level);	// expr {, expr }...
	if (indexes.empty())
		error("expected expression-list");

	unsigned nindexes = indexes.size();
	for (auto index : indexes) {			// process each index, in turn
		emit(OpCode::LLIMIT, 0, atype->range().minimum());
		emit(OpCode::ULIMIT, 0, atype->range().maximum());

		if (atype->itype()->tclass() != index->tclass()) {
			ostringstream oss;
			oss	<< "incompatable array index type, expected "
				<< atype->itype()->tclass() << " got " << index->tclass();
			error(oss.str());
					
		} else if (type->size() != 1) {		// scale the index, if necessary
			emit(OpCode::PUSH, 0, type->size());
			emit(OpCode::MUL);		// scale the index
		}

		// offset index for non-zero based arrays
		if (atype->range().minimum() != 0) {
			emit(OpCode::PUSH, 0, atype->range().minimum());
			emit(OpCode::SUB);
		}

		emit(OpCode::ADD);					// index into the array
				
		if (--nindexes) {					// link to next (base) if there's another index
			atype = type;					// The arrays type
			type = atype->base();			// We'll return the arrays base type...
		}
	}

	return type;
}

/********************************************************************************************//**
 * Emits a selector reference; the '.' has already been consumed
 *
 * @param	it		Variable's entry into the symbol table
 * @param 	type	The owning record type
 * @return	The identifier type
 ************************************************************************************************/
TDescPtr PComp::varSelector(SymbolTable::iterator it, TDescPtr type) {
	if (type->tclass() != TypeDesc::Record)
		error("attempted selector reference into non-record", it->first);

	// Copy, and then consume, the selector identifier...
	const string selector = ts.current().string_value;
	if (expect(Token::Identifier)) {
		unsigned offset = 0;				// Calc the offset into the record...
		for (const auto& fld : type->fields()) {
			if (fld.name() == selector) {
				type = fld.type();			// Return the record field's type
				break;
			}
			offset += fld.type()->size();
		}

		if (offset > 0) {				// Don't bother if it's the 1st field...
			emit(OpCode::PUSH, 0, offset);
			emit(OpCode::ADD);
		}
	}

	return type;
}

/********************************************************************************************//**
 * variable		  = identifier [ composite-desc { composite-desc } ] ;
 * composite-desc = '[' expression-lst ']' | '.' identifier ;
 *
 * Emits a variable reference, optionally with array indexes.
 *
 * @param	id		The variable or array identifier
 * @param	level	The current block level.
 * @return	The identifier type
 ************************************************************************************************/
TDescPtr PComp::variable(int level, SymbolTable::iterator it) {
	auto type = it->second.type();			// Default is the symbol's type

	emitVarRef(level, it->second);

	// process composite-desc's...
	for (;;) {
		if (accept(Token::OpenBrkt)) {		// variable is an array, index into it
			type = varArray(level, it, type);
			expect(Token::CloseBrkt);

		} else if (accept(Token::Period))	// handle record selector...
			type = varSelector(it, type);

		else if (accept(Token::Caret)) {	// Dereference a pointer
			emit(OpCode::EVAL, 0, type->size());	
			if (type->tclass() != TypeDesc::Pointer) {
				ostringstream oss;
				oss << "expected a pointer, got " << type->tclass();
				error(oss.str());
			} else
				type = type->base();		// Use the pointed to type

		} else
			break;							// no more composite-desc(s)
	}

	return type;
}

/********************************************************************************************//**
 * variable := expression 
 *
 * @param	level	The current block level.
 * @param	it		The variable, or function, identifier
 * @param	dup		Duplicate reference to the variable if true
 *
 * @return	Reference to the variable reference
 ************************************************************************************************/
void PComp::assignStatement(int level, SymbolTable::iterator it, bool dup) {
	auto type = it->second.type();		// Emit the lvalue...

	// Emit the l-value, a variable or a function reference...
	if (it->second.kind() == SymValue::Function)
		emit(OpCode::PUSHVAR, 0, FrameRetVal);

	else if (it->second.kind() == SymValue::Variable)
		type = variable(level, it);

	else
		error("expected a variable or a function, got:", it->first);

	if (dup)
		emit(OpCode::DUP);

	expect(Token::Assign);

	// Emit the r-value and assignment...

	auto rtype = expression(level);
	assignPromote(type, rtype);
	emit(OpCode::ASSIGN, 0, type->size());
}

/********************************************************************************************//**
 * variable := expression | identifier [ '(' expression-list ')' ]
 *
 * Emits code for an assignment statement, or a procedure call...
 *
 * @param	level	The current block level.
 * @param	id		The variable or prodecure identifier
 ************************************************************************************************/
void PComp::identStatement(int level, const string& id) {
	auto lhs = lookup(id);
	if (lhs == symtbl.end())
		return;							// id is unidentified

	switch(lhs->second.kind()) {
	case SymValue::Procedure:			// emit a procedure call
		callStatement(level, lhs);
		break;

	case SymValue::Function:			// emit reference to function return value
	case SymValue::Variable:			// emit a reference to a variable, or array, value
		assignStatement(level, lhs);
		break;

	case SymValue::Constant:
		error("Can't assign to a constant", lhs->first);
		break;

	default:
		error("expected variable, function return ref, or procedure call, got", lhs->first);
	}
}

/********************************************************************************************//**
 * write or writeln[ format-list ]
 *
 * Process write and writeln parameters, up to, but not including, emitteing the final op-code.
 *
 * @note	Does not handle arrays, as that would require some sort of tagged reference, which
 *			Datum lacks.
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::writeStmt(int level) {
	ostringstream oss;

	unsigned nargs = 0;
	if (accept(Token::OpenParen)) {				// process, and count, each expr-tuple..
		do {
			auto expr = expression(level);		// value to write

			if (accept(Token::Colon)) {			// [ ':' width [ ':' precision ]]
				auto width = expression(level);
				if (width->tclass() != TypeDesc::Integer) {
					oss << "expeced integer width parameter, got: " << width->tclass();
					error(oss.str());
				}

				if (accept(Token::Colon)) {		//	[ ':' precision ]
					auto prec = expression(level);
					if (prec->tclass() != TypeDesc::Integer) {
						oss << "expeced integer width parameter, got: " << width->tclass();
						error(oss.str());
					}

				} else
					emit(OpCode::PUSH, 0, 0);	// push default precision
						
			} else {							// push default width & precision
				emit(OpCode::PUSH, 0, 0);
				emit(OpCode::PUSH, 0, 0);
			}

			++nargs;
		} while (accept(Token::Comma));

		expect(Token::CloseParen);
	}

	emit(OpCode::PUSH, 0, nargs);
}

/********************************************************************************************//**
 * write [ format-list ]
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::writeStatement(int level) {
	writeStmt(level);
	emit(OpCode::WRITE);
}

/********************************************************************************************//**
 * writeln [ format-list ]
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::writeLnStatement(int level) {
	writeStmt(level);
	emit(OpCode::WRITELN);
}

/********************************************************************************************//**
 * new(identifier)
 *
 * @note	Totken::New has already been consumed.
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::statementNew(int level) {
	expect(Token::OpenParen);

	const string id = ts.current().string_value;
	if (expect(Token::Identifier)) {
		auto it = lookup(id);
		TDescPtr tdesc = TypeDesc::intDesc;
		if (it != symtbl.end())
			tdesc = variable(level, it);

		if (tdesc->tclass() != TypeDesc::Pointer) {
			ostringstream oss;
			oss << "expected a pointer, got " << tdesc->tclass();
			error(oss.str());
		}

		emit(OpCode::PUSH, 0, tdesc->size());	// push the size of the id
		emit(OpCode::NEW);
		emit(OpCode::ASSIGN, 0, 1);

		expect(Token::CloseParen);
	}
}

/********************************************************************************************//**
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::statementProcs(int level) {
	ostringstream oss;

	if (accept(Token::Write))						// write [ '(' expr-tuple { ',' expr-tuple } ')' ]
		writeStatement(level);

	else if (accept(Token::Writeln))				// writeln [ '(' expr-tuple { ',' expr-tuple } ')' ]
		writeLnStatement(level);

	else if (accept(Token::New))					// 'New (' id ')'
		statementNew(level);

	else if (accept(Token::Dispose)) {				// 'Dispose (' expr ')'
		expect(Token::OpenParen);
		auto tdesc = expression(level);
		if (tdesc->tclass() != TypeDesc::Pointer) {
			ostringstream oss;
			oss << "expected a pointer, got " << tdesc->tclass();
			error(oss.str());
		}
		emit(OpCode::DISPOSE);
		expect(Token::CloseParen);
		
	}

	// else: nothing
}

/********************************************************************************************//**
 * [ ident = expr						|
 *   if expr then stmt { else stmt }	|
 *   while expr do stmt					|
 *   repeat stmt until expr				|
 *   stmt-block ]
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::statement(int level) {
	ostringstream oss;

	if (accept(Token::Identifier, false)) {		// Assignment or procedure call
		// Copy and then consume the l-value identifier...
		const string id = ts.current().string_value;
		next();
		identStatement(level, id);

	} else if (accept(Token::Begin)) {				// begin ... end
		statementList(level);
		expect(Token::End);

	} else if (accept(Token::If)) 					// if expr then stmt { then stmt... }
		ifStatement(level);

	else if (accept(Token::While))					// while expr do stmt...
		whileStatement(level);

	else if (accept(Token::Repeat))					// "repeat" stmt until expr...
		repeateStatement(level);

	else if (accept(Token::For))					// 'for' var ':=' expr ( 'to' | 'downto') expr 'do' stmt...
		forStatement(level);

	else
		statementProcs(level);
}

/********************************************************************************************//**
 * const const-decl { ; const-decl } ;
 *
 * @note	Doesn't emit any code; just stores the named value in the symbol table.
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::constDeclList(int level) {
	// Stops if the ';' if followd by any of hte following tokens
	static const Token::KindSet stops {
		Token::TypeDecl,
		Token::VarDecl,
		Token::ProcDecl,
		Token::FuncDecl,
		Token::Begin
	};

	if (accept(Token::ConsDecl)) {
		do {
			if (oneOf(stops))
				break;							// No more constants...
			constDecl(level);

		} while (accept(Token::SemiColon));
	}
}

/********************************************************************************************//**
 * ident = type
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::typeDecl(int level) {
	const auto ident = nameDecl(level);				// Copy the identifier
	expect(Token::EQU);								// Consume the "="
	auto tdesc = type(level, ident);

	if (verbose)
		cout << progName << ": type " << ident << " = " << tdesc->tclass() << "\n";

	symtbl.insert(	{ ident, SymValue::makeType(level, tdesc) }	);
}

/********************************************************************************************//**
 * type type-decl { ; type-decl } ;
 *
 * @note	Doesn't emit any code; just stores the new type in the symbol table.
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::typeDeclList(int level) {
	// Stops if the ';' if followd by any of hte following tokens
	static const Token::KindSet stops {
		Token::VarDecl,
		Token::ProcDecl,
		Token::FuncDecl,
		Token::Begin
	};

	if (accept(Token::TypeDecl)) {
		do {
			if (oneOf(stops))
				break;							// No more constants...
			typeDecl(level);

		} while (accept(Token::SemiColon));
	}
}

/********************************************************************************************//**
 * ident = const-expr ;
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::constDecl(int level) {
	auto ident = nameDecl(level);				// Copy the identifier
	expect(Token::EQU);							// Consume the "="
	auto value = constExpr();					// Get the constant value

	if (!value.first)
		error("expected a const-expression, got:", ts.current().string_value);

	TDescPtr type = value.second.kind() == Datum::Kind::Integer ? TypeDesc::intDesc : TypeDesc::realDesc;

	symtbl.insert(	{ ident, SymValue::makeConst(level, value.second, type) } );
	if (verbose)
		cout << progName << ": constDecl " << ident << ": " << level << ", " << value.second << "\n";
}

/********************************************************************************************//**
 * const" var-decl-lst ;
 *
 * @param	level	The current block level.
 * @return  Number of variables allocated before or after the activation frame.
 ************************************************************************************************/
int PComp::varDeclBlock(int level) {
	FieldVec	idents;							// vector of name/type pairs.

	if (accept(Token::VarDecl))
		varDeclList(level, false, "", idents);

	int sum = 0;								// Add up the size of every variable in the block
	for (const auto& id : idents)
		sum += id.type()->size();

	return sum;
}

/********************************************************************************************//**
 * var-decl { ';' var-decl }
 *
 * Allocate space on the stack for each variable, as a postivie offset from the end of current
 * activaction frame. Create a new entry in the symbol table, that notes the offset and data
 * type.
 *
 * @param			level	The current block level.
 * @param			params	True if processing formal parameters, false if variable declaractions. 
 * @param			prefix	The identifier prefix
 * @param[in,out]	idents	Vector of identifer, kind pairs 
 *
 * @return  Offset of the next variable/parmeter from the current activation frame.
 ************************************************************************************************/
void PComp::varDeclList(int level, bool params, const string& prefix, FieldVec& idents) {
	// Stops if the ';' if followd by any of hte following tokens
	static const Token::KindSet stops {
		Token::ProcDecl,
		Token::FuncDecl,
		Token::Begin,
		Token::CloseParen
	};

	do {
		if (oneOf(stops))
			break;								// No more variables...
		varDecl(level, prefix, idents);
	} while (accept(Token::SemiColon));

	/*
	 * Set the starting offset from the activation frame. Locals have positive offsets, while
	 * parameters have negative offsets in reverse. 
	 */

	int dx = 0;
	if (params)
		for (const auto& id : idents)
			dx -= id.type()->size();

	for (const auto& id : idents) {				// install the results in the symbol table...
		if (verbose)
			cout << progName  
				 << ": var/param " 	<< id.name() << ": " 
				 << level 			<< ", "
			     << dx	 			<< ", " 
				 << id.type()->tclass() << "\n";

		auto range = symtbl.equal_range(id.name());	// Already defined?
		for (auto it = range.first; it != range.second; ++it)
			if (it->second.level() == level)
				error("previously defined", id.name());

		symtbl.insert( { id.name(), SymValue::makeVar(level, dx, id.type())	} );
		dx += id.type()->size();
	}
}

/********************************************************************************************//**
 * A semicolon list of variable, or formal parameter declractions. Each declaraction starts
 * with a comma separated list of identifiers, followed by a colon followed by a type
 * specifier:
 *
 *     var-decl =	    ident-list : type ;
 *     ident-list =     ident { "," ident } ;
 *
 * Allocate space on the stack for each variable, as a positive offset from the *end* of
 * current activaction frame; 0, 1, ..., n-1. Parameters, pushed by the caller, are identified
 * by negative indexes from *before the start* of the frame; -1, -2, ..., -n. Create a new
 * entry in the symbol table for either.
 *  
 * @param			level	The current block level. 
 * @param			prefix	The identifer prefix
 * @param[in,out]	idents	Vector of identifer, kind pairs
 ************************************************************************************************/
void PComp::varDecl(int level, const string& prefix, FieldVec& idents) {
	vector<string> ids = identifierList(level, prefix);
	expect(Token::Colon);
	const auto desc = type(level, prefix);
	for (auto& id : ids)
		idents.push_back({ id, desc });
}

/********************************************************************************************//**
 * identifier-lst : identifer { ',' identifier }
 * @param			level	The current block level. 
 * @return	List of identifiers in the identifier-lst
 ************************************************************************************************/
vector<string> PComp::identifierList(int level, const string& prefix) {
	vector<string> ids;

	do {
		ids.push_back(nameDecl(level, prefix));
	} while (accept(Token::Comma));

	return ids;
}

/********************************************************************************************//**
 * simple-type | structured-type | pointer-type ;
 *
 * Previously defined types, as well as  built-in types such as "Integer" and "Real" will have
 * Token::Type, thus we only need to look for new type declaractions, e.g., "array...".
 *
 * @param	level	The current block level. 
 * @param	prefix	Optional identifier prefix. Defaults to "".
 * @return the type description
 ************************************************************************************************/
TDescPtr PComp::type(int level, const string& prefix) {
	auto tdesc = TypeDesc::intDesc;

	if (accept(Token::Identifier, false)) { 	// previously defined type-name
		const string id = ts.current().string_value;
		next();

		auto it = lookup(id);
		if (it == symtbl.end() || it->second.kind() != SymValue::Type)
			error("expected type, got ", id);
		else
			tdesc = it->second.type();

	} else if (accept(Token::Caret)) 			// Pointer type
		tdesc = TypeDesc::newPointerDesc(type(level, prefix));

	else if ((tdesc = structuredType(level, prefix)) != 0)
		;

	else 
		tdesc = simpleType(level);

	return tdesc;
}

/********************************************************************************************//**
 * integer' | '(' ident-list ')' | const-expr '..' const-expr
 *
 * @note Needs to handle previously defined simple-types
 * 
 * @param	level	The current block level. 
 * @return the type description
 ************************************************************************************************/
TDescPtr PComp::simpleType(int level) {
	TDescPtr type;

	if (accept(Token::Identifier, false)) {		// Previously defined type, must be ordinal!
		const string id = ts.current().string_value;
		next();

		auto it = lookup(id);
		if (it == symtbl.end() || it->second.kind() != SymValue::Type)
			error("expected type, got ", it->first);
		else if (!it->second.type()->isOrdinal())
			error("expected ordinal type, got ", it->first);
		else
			type = it->second.type();

	} else if (accept(Token::RealType))			// Real
		type = TypeDesc::realDesc;

	else 
		type = ordinalType(level);

	return type;
}

/********************************************************************************************//**
 * 'Boolean' | 'Integer' | 'Char' |
 * '(' identifier-list ')' | 'array' '[' simple-type-list ']' 'of' type ;
 *
 * @param	level	The current block level. 
 * @return type description if successful, null pointer otherwise
 ************************************************************************************************/
TDescPtr PComp::ordinalType(int level) {
	TDescPtr type;

	if (accept(Token::BoolType))				// Boolean
		return TypeDesc::boolDesc;

	else if (accept(Token::CharType))			// Char
		return TypeDesc::charDesc;

	else if (accept(Token::IntType))			// Integer
		return TypeDesc::intDesc;

	else if (accept(Token::OpenParen)) {		// Enumeration
		FieldVec		enums;

		const auto ids = identifierList(level, "");
		SubRange r(0, ids.empty() ? 0 : ids.size()-1);
		expect(Token::CloseParen);

		// Create the type, excluding the fields (enumerations)
		type = TypeDesc::newEnumDesc(r);

		unsigned value = 0;						// Each enumeration gets a value...
		for (auto id : ids) {
			enums.push_back( { id, TypeDesc::intDesc } );
			symtbl.insert(	{ id, SymValue::makeConst(level, Datum(value), type) }	);
			if (verbose)
				cout << progName << ": enumeration '" << id << "' = " << value << ", " << level << "\n";
			++value;
		}

		type->fields(enums);

	} else {									// Sub-Range
		auto minValue = constExpr();
		if (minValue.first) {
			ostringstream oss;

			expect(Token::Ellipsis);
			auto maxValue = constExpr();
			if (!maxValue.first) {
				oss << "expected constant expression, got: " << current();
				error(oss.str());
				maxValue.second = minValue.second;
			}

			if (minValue.second > maxValue.second) {
				oss << "Minimum sub-range value (" << minValue.second << ")"
					<< " is greater than the maximum value (" << maxValue.second << ")";
				error(oss.str());
				swap(minValue.second, maxValue.second);

			} else if (	minValue.second.kind() != Datum::Kind::Integer		||
						maxValue.second.kind() != Datum::Kind::Integer) {
				oss << "Both sub-range values must be ordinal types; "
					<< minValue.second << ", " << maxValue.second;
				error(oss.str());
				minValue.second = 0; maxValue.second = 1;
			}

			SubRange r(minValue.second.integer(), maxValue.second.integer());
			type = TypeDesc::newIntDesc(r);
		}
	}

	return type;
}

/********************************************************************************************//**
 * 'array' '[' simple-type-list ']' 'of' type | 'record' field-list 'end' ;
 *
 * @param	level	The current block level. 
 * @param	previx	Optional identifier prefix
 * @return type description if successful, null pointer otherwise
 ************************************************************************************************/
TDescPtr PComp::structuredType(int level, const string& prefix) {
	TDescPtr tdesc = 0;

	if (accept(Token::Array)) {					// Array
		expect(Token::OpenBrkt);				// "["

		TDescPtr tp;
		TDescPtrVec indexes = simpleTypeList(level);
		for (auto index : indexes) {
			const SubRange r = index->range();
			tdesc = TypeDesc::newArrayDesc(r.span(), r, index);
			if (tp == 0)						// Remember the first array type descriptior...
				tp = tdesc;

			else {								// Following indexes...
				tp->size(tp->size() * r.span());
				tp = tp->base();
			}
		}

		expect(Token::CloseBrkt);				// "] of"
		expect(Token::Of);

		tp->base(type(level, ""));				// Get the array's base type
		tp->size(tp->size() * tp->base()->size());

	} else if (accept(Token::Record)) {			// Record
		FieldVec	fields;
		fieldList(level, prefix, fields);

		unsigned sum = 0;						// Calc the total size of the record
		for (auto& element : fields)
			sum += element.type()->size();

		tdesc = TypeDesc::newRcrdDesc(sum, fields);

		expect(Token::End);
	}

	return tdesc;
}

/********************************************************************************************//**
 * var-decl-list 
 *
 * @param			level	The current block level.
 * @param			prefix	The identifier prefix
 * @param[in,out]	fields	Vector of identifer, kind pairs. identifier arn't decorated.
 *
 * @return  Offset of the next variable/parmeter from the current activicaqtion frame.
 ************************************************************************************************/
void PComp::fieldList(int level, const string& prefix, FieldVec& fields) {
	varDeclList(level, false, prefix, fields);

	for (auto& fld : fields) {					// trim off the prefix '.' from the identifiers
		size_t n = fld.name().find('.');
		if (n != string::npos && n < fld.name().size()) {
			string name = fld.name();
			fld.name(name.erase(0, n+1));
		}
	}
}

/********************************************************************************************//**
 * simple-type-lst: simple-type { ',' simple-type } ;
 *
 * @param	level	The current block level. 
 ************************************************************************************************/
TDescPtrVec PComp::simpleTypeList(int level) {
	TDescPtrVec tdescs;

	do {
		tdescs.push_back(simpleType(level));
	} while (accept(Token::Comma));

	return tdescs;
}

/********************************************************************************************//**
 * Process the common subroutine delcaration for procedures and functions:
 *
 * ident [ ( var-decl-lst ) ] ) ...
 *
 * @param	level	The current block level.
 * @param 	kind	The type of subroutine, e.g., procedure or fuction
 * @return	subrountine's symbol table entry
 ************************************************************************************************/
SymValue& PComp::subPrefixDecl(int level, SymValue::Kind kind) {
	SymbolTable::iterator	it;				// Will point to the new symbol table entry...

	auto ident = nameDecl(level);			// insert the name into the symbol table
	it = symtbl.insert( { ident, SymValue::makeSbr(kind, level)	} );
	if (verbose)
		cout << progName << ": subPrefixDecl " << ident << ": " << level << ", 0\n";

	// Process the formal auguments, if any...
	if (accept(Token::OpenParen)) {
		// Note that the activation frame level is that of the *following* block!

		FieldVec	idents;					// vector of name/type pairs.
		varDeclList(level+1, true, "", idents);
		expect(Token::CloseParen);

		for (auto id : idents)
			it->second.params().push_back(id.type());
	}

	return it->second;
}

/********************************************************************************************//**
 * procedure ident [ ( var-decl-lst ) ] ; block ;
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::procDecl(int level) {
	auto& val = subPrefixDecl(level, SymValue::Procedure);
	expect(Token::SemiColon);
	blockDecl(val, level + 1);
	expect(Token::SemiColon);				// procedure declarations end with a ';'!
}

/********************************************************************************************//**
 * function ident [ ( var-decl-lst ) ] : type  block-decl ;
 *
 * @param	level	The current block level.
 ************************************************************************************************/
void PComp::funcDecl(int level) {
	auto& val = subPrefixDecl(level, SymValue::Function);
	expect(Token::Colon);
	val.type(type(level, ""));
	expect(Token::SemiColon);
	blockDecl(val, level + 1);
	expect(Token::SemiColon);	// function declarations end with a ';'!
}

/********************************************************************************************//**
 * proc-decl | func-decl
 *
 * Zero or more function and/or procedure delclaractions:
 *
 *     { proc-decl | funct-decl }
 *     proc-decl =      "procedure" ident param-decl-lst block-decl ";" ;
 *     func-decl =      "function"  ident param-decl-lst ":" type block-decl ";" ;
 *     type ; type =    "Integer" | "Real" ;
 *
 * @param level The current block level.
 ************************************************************************************************/
void PComp::subDeclList(int level) {
	for (;;) {
		if (accept(Token::ProcDecl))
			procDecl(level);

		else if (accept(Token::FuncDecl))
			funcDecl(level);

		else
			break;
	}
}

/********************************************************************************************//**
 * [ const-decl-lst ]
 * [ var-decl-blk   ]
 * [ sub-decl-lst   ]
 * [ stmt-block     }
 *
 * @param	val		The blocks (procedures) symbol table entry value
 * @param	level	The current block level.
 * @return 	Entry point address
 ************************************************************************************************/
unsigned PComp::blockDecl(SymValue& val, int level) {
	constDeclList(level);						// declaractions...
	typeDeclList(level);
	auto dx = varDeclBlock(level);
	subDeclList(level);

	/* Block body
	 *
	 * Emit the block's prefix, saving and return its address, followed by the postfix. Omit the prefix
	 * if dx == 0 (the subroutine has zero locals.
	 */

	const auto addr = dx > 0 ? emit(OpCode::ENTER, 0, dx) : code->size();
	val.value(addr);

	if (expect(Token::Begin)) {					// "begin" statements... "end"
		statementList(level);
		expect(Token::End);
	}

	// block postfix...
	const auto sz = val.params().size();
	if (SymValue::Function == val.kind())
		emit(OpCode::RETF, 0, sz);	// function...
	else
		emit(OpCode::RET, 0, sz);	// procedure...

	purge(level);								// Remove symbols only visible at this level

	return addr;
}

/********************************************************************************************//**
 * @param	level	Top level block number
 ************************************************************************************************/
void PComp::progDecl(int level) {
	next();										// Fetch the 1st token

	expect(Token::ProgDecl);					// Program heading...
	auto& val = subPrefixDecl(level, SymValue::Procedure);
	expect(Token::SemiColon);

	// Emit a call to the main procedure, followed by a halt
	const auto call_pc = emit(OpCode::CALL, level, 0);
	emit(OpCode::HALT);

	const auto addr = blockDecl(val, level);	// emit the first block 
	if (verbose)
		cout << progName << ": patching call to program at " << call_pc << " to " << addr  << "\n";

	(*code)[call_pc].addr = addr;

	expect(Token::Period);
}

/********************************************************************************************//**
 ************************************************************************************************/
void PComp::run()								{	progDecl(0);	}

/************************************************************************************************
 * public:
 ************************************************************************************************/

/********************************************************************************************//**
 * Construct a new compilier with the token stream initially bound to std::cin.
 ************************************************************************************************/
PComp::PComp() : Compilier () {

	// Insert builtin types into the symbol table

	symtbl.insert( { "bool",	SymValue::makeType(0, TypeDesc::boolDesc)	} );
	symtbl.insert( { "char",	SymValue::makeType(0, TypeDesc::charDesc)	} );
	symtbl.insert( { "integer",	SymValue::makeType(0, TypeDesc::intDesc)	} );
	symtbl.insert( { "real",	SymValue::makeType(0, TypeDesc::realDesc)	} );

	// Insert built-in constants into the symbol table; id, level (always zero), and value

	symtbl.insert({"maxint", 	SymValue::makeConst(0, TypeDesc::maxRange.maximum(), TypeDesc::intDesc)	});
	symtbl.insert({"nil",    	SymValue::makeConst(0, 0, TypeDesc::newPointerDesc(TypeDesc::intDesc))	});
	symtbl.insert({"true",   	SymValue::makeConst(0, true, TypeDesc::boolDesc)						});
	symtbl.insert({"false",  	SymValue::makeConst(0, false, TypeDesc::boolDesc)						});
}

