// operation codes
#pragma once

//-------------------------------------------------------------------------------------------------
// op codes
enum Op : byte
{
	PUSH_TRUE,
	PUSH_FALSE,
	PUSH_VAR,
	PUSH_ARG,
	PUSH_CHAR,
	PUSH_CSTR,
	PUSH_INT,
	PUSH_FLOAT,
	POP,
	SET_VARS,
	SET_VAR,
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	NEG,
	CALL,
	CALLF,
	CAST,
	RET,
	CMP,
	TEST,
	JMP,
	JE,
	JNE,
	JG,
	JGE,
	JL,
	JLE,
	JES,
	JNES,
	JGS,
	JGES,
	JLS,
	JLES
};

//-------------------------------------------------------------------------------------------------
// return priority of arthmetic operators
inline int GetPriority(int op)
{
	if(op == MUL || op == DIV || op == MOD)
		return 2;
	else if(op == ADD || op == SUB)
		return 1;
	else
		return 0;
}

//-------------------------------------------------------------------------------------------------
// return character representing operation, used for debug output
inline cstring OpChar(int op)
{
	switch(op)
	{
	case ADD:
		return "+";
	case SUB:
		return "-";
	case MUL:
		return "*";
	case DIV:
		return "/";
	case MOD:
		return "%";
	case JE:
		return "==";
	case JNE:
		return "!=";
	case JG:
		return ">";
	case JGE:
		return ">=";
	case JL:
		return "<";
	case JLE:
		return "<=";
	default:
		return "?";
	}
}

//-------------------------------------------------------------------------------------------------
struct OpInfo
{
	cstring name;
	enum Arg
	{
		A_NONE,
		A_CHAR,
		A_BYTE,
		A_SHORT,
		A_INT,
		A_FLOAT
	} arg;
};
extern const OpInfo op_info[];
