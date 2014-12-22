#pragma once

//-------------------------------------------------------------------------------------------------
#include "Var.h"
#include "ScriptFunction.h"

//-------------------------------------------------------------------------------------------------
// program stack, required by functions
extern vector<Var> stack;

//-------------------------------------------------------------------------------------------------
// run program
void run(byte* code, vector<Str*>& strs, vector<ScriptFunction>& sfuncs);
void try_run(byte* code, vector<Str*>& strs, vector<ScriptFunction>& sfuncs);
