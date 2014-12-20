#pragma once

//-------------------------------------------------------------------------------------------------
#include "Var.h"

//-------------------------------------------------------------------------------------------------
// function pointer
typedef void (*VoidF)();

//-------------------------------------------------------------------------------------------------
// max arguments function can have
#define MAX_ARGS 2

//-------------------------------------------------------------------------------------------------
// function type
struct Function
{
	cstring name;
	VAR return_type;
	int arg_count;
	VAR args[MAX_ARGS];
	VoidF f;
};

//-------------------------------------------------------------------------------------------------
// array of functions
extern const Function funcs[];
extern const int n_funcs;

//-------------------------------------------------------------------------------------------------
// _getch replacement
void pause();