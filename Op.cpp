#include "Pch.h"
#include "Base.h"
#include "Op.h"

const OpInfo op_info[] = {
	"PUSH_VAR", OpInfo::A_BYTE,
	"PUSH_ARG", OpInfo::A_BYTE,
	"PUSH_CSTR", OpInfo::A_BYTE,
	"PUSH_INT", OpInfo::A_INT,
	"PUSH_FLOAT", OpInfo::A_FLOAT,
	"POP", OpInfo::A_NONE,
	"SET_VARS", OpInfo::A_BYTE,
	"SET_VAR", OpInfo::A_BYTE,
	"ADD", OpInfo::A_NONE,
	"SUB", OpInfo::A_NONE,
	"MUL", OpInfo::A_NONE,
	"DIV", OpInfo::A_NONE,
	"MOD", OpInfo::A_NONE,
	"NEG", OpInfo::A_NONE,
	"CALL", OpInfo::A_BYTE,
	"CALLF", OpInfo::A_BYTE,
	"CAST", OpInfo::A_BYTE,
	"RET", OpInfo::A_NONE,
	"CMP", OpInfo::A_NONE,
	"JMP", OpInfo::A_SHORT,
	"JE", OpInfo::A_SHORT,
	"JNE", OpInfo::A_SHORT,
	"JG", OpInfo::A_SHORT,
	"JGE", OpInfo::A_SHORT,
	"JL", OpInfo::A_SHORT,
	"JLE", OpInfo::A_SHORT,
};
