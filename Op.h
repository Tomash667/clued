// operation codes
#pragma once

//-------------------------------------------------------------------------------------------------
// op codes
enum Op : byte
{
	PUSH_VAR,
	PUSH_ARG,
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
	JMP,
	JE,
	JNE,
	JG,
	JGE,
	JL,
	JLE
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
