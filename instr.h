/********************************************************************************************//**
 * @file instr.h
 *
 * P machine operation codes, machine instruction format, activation frame format, and
 * associated utilities used by both the compiler (PComp) and the interpreter (PInterp).
 *
 * @author Randy Merkel, Slowly but Surly Software.
 * @copyright  (c) 2017 Slowly but Surly Software. All rights reserved.
 ************************************************************************************************/

#ifndef	INSTR_H
#define INSTR_H

#include <map>
#include <string>
#include <vector>

#include "datum.h"

/********************************************************************************************//**
 * Activation Frame layout
 *
 * Word offsets from the start of a activaction frame, as created by OpCode::Call. Note that
 * procedures and function have the same sized frame.
 ************************************************************************************************/
enum Frame {
	FrameBase		= 0,				///< Offset to the Activation Frame base (base(n))
	FrameOldFp		= 1,				///< Offset to the saved frame pointer register
	FrameRetAddr	= 2,	  			///< Offset to the return address
	FrameRetVal		= 3,				///< Offset to the function return value

	FrameSize							///< Number of entries in an activaction frame (4)
};

/********************************************************************************************//**
 * Machine operation codes
 *
 * OP level, addr - description; definition: error
 ************************************************************************************************/
enum class OpCode : unsigned char {
	NEG,		///< NEG - Negate; push(pop() * -1)
	ITOR,		///< ITOR - Convert integer to real; push(Real(pop()))
	ITOR2,		///< ITOR2 - Convert integer to real; push(Real(pop()))
	ROUND,		///< ROUND - Round real to nearest integer; push(Round(pop()))
	TRUNC,		///< TRUNC - Truncate real to integer; push(Truncate(pop()))
	ABS,		///< ABS - Absolute value; push(Abs(pop()))
	ATAN,		///< ATAN - Arc tangent; push(ATan(pop()))
	EXP,		///< EXP - Exponent; push(Exp(pop()))
	LOG,		///< LOG - Natural logarithm; push(Log(pop()))

	DUP,		///< DUP - Duplicate; Push(stack[sp])
	ODD,		///< ODD - Is odd?; Push(IsOdd(pop()))
	PRED,		///< PRED ,limit - Predecessor; push(pop() - 1); OutOfRange if TOS was <= limit
	SUCC,		///< SUCC ,limit - Successor; push(pop() + 1); OutOfRange if TOS was >= limit

	SIN,		///< SIN   - Sine; push(Sin(pop()))
	SQR,		///< SQR   - Square; push(stack[sp] * pop())
	SQRT,		///< SQRT  - Square-root; push(Sqrt(pop()))

	WRITE,		///< WRITE - Write values on standard output; for n=pop(); n>0; --n write pop()
	WRITELN,	///< WRITELN - Write values followed by newline on standard output
	NEW,		///< NEW   - Allocate dynamic store; n=pop(); allocate n dataums, push(addr) or zero if insufficient space
	DISPOSE,	///< DISPOSE - Dispose of allocated dynamic store; free pop()

	ADD,		///< ADD - Addition; push(pop() + pop())
	SUB,		///< SUB - Subtraction; r = pop(); push(pop - r)
	MUL,		///< MUL - Multiplication; push(pop() * pop())
	DIV,		///< DIV - Division; r = pop(); push(pop() * r)
	REM,		///< REM - Remainder; r = pop(); push(pop() % r)

	LT,			///< LT - Less than; r = pop(); push(pop() < r)
	LTE,		///< LTE - Less then or equal; push(pop() > pop()
	EQU,		///< EQU - Is equal to; push(pop() == pop())
	GTE,		///< GTE - Greater than or equal; push(pop() < pop()
	GT,			///< GT - Greater than; push(pop() <= pop()
	NEQU,		///< NEQU - Does not equal; push(pop() != pop())

	LOR,		///< LOR - Logical or; push(pop() || pop())
	LAND,		///< LAND - Logical and; push(pop() && pop())
	LNOT,		///< LNOT - Logical not; push(!pop())
	
	POP,		///< POP ,n - Pop Datums into the bit bucket; for i = 0; i < addr; ++i, pop()
	PUSH,		///< PUSH , const - Push a constant integer value; push(const)
	PUSHVAR,	///< PUSHVAR level,offset - Push variable address; push(base(level) + offset)
	EVAL,		///< EVAL ,n - Evaluate variable; variable address is TOS, variable size is n Datums
	ASSIGN,		///< ASSIGN ,n - Assign stack(TOS-n,TOS) to stack[addr,addr+n), POP ,n
	COPY,		///< COPY ,n - Copy Datums; dest=pop(); src=pop(); copy n Datums from src to dest

	CALL,		///< CALL level,addr - Call a procedure, pushing a new acrivation Frame
	ENTER,		///< ENTER ,n - Allocate n locals on the stack
	RET,		///< Return from procedure; unlink Frame
	RETF,		///< Return from function; push result
	JUMP,		///< Jump to a location
	JNEQ,		///< Condition = pop(); Jump if condition == false (0)

	LLIMIT,		///< Check array index; out-of-range error if TOS <  addr
	ULIMIT,		///< Check array index; out-of-range error if TOS >  addr

	HALT = 255	///< Halt the machine
};

/********************************************************************************************//**
 * OpCode Information
 *
 * An OpCodes name string, and the number of stack elements it accesses
 ************************************************************************************************/
class OpCodeInfo {
	/// An OpCode to OpCodeInfo mapping
	typedef std::map<OpCode, OpCodeInfo>	InfoMap;

	static const InfoMap	opInfoTbl;	///< A table of OpCode information

	std::string		_name;				///< The OpCodes name, e.g., "add"
	unsigned		_nElements;			///< Number of stack elements used , e.g, 2

public:
	OpCodeInfo() : _nElements{0} {}		///< Construct an empty element

	/// Construct a OpCodeInfo from it's components
	OpCodeInfo(const std::string& name, unsigned nelements)
		: _name{name}, _nElements{nelements} {}

	virtual ~OpCodeInfo() {}			///< Destructor

	/// Return the OpCode name string
	const std::string name() const		{	return _name;   	}

	/// Return the number of stack elements the OpCode uses
	unsigned nElements() const			{	return _nElements;  }

	/// Return information about an OpCode
	static const OpCodeInfo& info(OpCode op);	///< Return information about op
};

/********************************************************************************************//**
 * An Instruction
 ************************************************************************************************/
struct Instr {
	Datum			addr;				///< A data value or memory address
	int8_t			level;				///< Base level: 0..255
	OpCode			op;					///< Operation code

	/// Default constructor; results in pushConst 0, 0...
	Instr() : level{0}, op{OpCode::HALT} {}

	/// Construct an instruction from it's components...
	Instr(OpCode o, int8_t l = 0, Datum d = 0) : addr{d}, level{l}, op{o} {}
};

/********************************************************************************************//**
 * A vector of Instr's (instructions)
 ************************************************************************************************/
typedef std::vector<Instr>					InstrVector;

/********************************************************************************************//**
 * Disassemble an instruction...
 ************************************************************************************************/
unsigned disasm(std::ostream& out, unsigned loc, const Instr& instr, const std::string label = "");

#endif

