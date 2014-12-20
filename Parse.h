#pragma once

//-------------------------------------------------------------------------------------------------
#include "Str.h"

//-------------------------------------------------------------------------------------------------
// output from parser required to run code
struct ParseOutput
{
	vector<byte> code;
	vector<Str*> strs;
};

//-------------------------------------------------------------------------------------------------
// parse code from file
bool parse(cstring file, ParseOutput& out);
