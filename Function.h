#pragma once

//-------------------------------------------------------------------------------------------------
#include "Var.h"

//-------------------------------------------------------------------------------------------------
// function pointer
typedef void (*VoidF)();

//-------------------------------------------------------------------------------------------------
// function type
struct Function
{
	cstring name;
	VAR return_type;
	vector<VAR> args;
	VoidF f;
};

//-------------------------------------------------------------------------------------------------
// array of functions
extern const Function funcs[];
extern const int n_funcs;

//-------------------------------------------------------------------------------------------------
// _getch replacement
void pause();

//-------------------------------------------------------------------------------------------------
void call_func(int id);
