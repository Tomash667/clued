#pragma once

//-------------------------------------------------------------------------------------------------
#include "Str.h"
#include "ScriptFunction.h"

//-------------------------------------------------------------------------------------------------
// output from parser required to run code
struct ParseOutput
{
	vector<byte> code;
	vector<Str*> strs;
	vector<ScriptFunction> funcs;
};

//-------------------------------------------------------------------------------------------------
// parse code from file
bool parse(cstring file, ParseOutput& out);
