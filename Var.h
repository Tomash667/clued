#pragma once

//-------------------------------------------------------------------------------------------------
#include "Str.h"

//-------------------------------------------------------------------------------------------------
// variable type
enum VAR
{
	V_VOID,
	V_INT,
	V_FLOAT,
	V_STRING
};

//-------------------------------------------------------------------------------------------------
// variable names
extern cstring var_name[];

//-------------------------------------------------------------------------------------------------
// check if can cast from one type to another
inline bool CanCast(VAR to, VAR from)
{
	if(from == to)
		return true;

	switch(to)
	{
	case V_VOID:
	case V_INT:
		if(from == V_FLOAT)
			return true;
		return false;
	case V_FLOAT:
		if(from == V_INT)
			return true;
		return false;
	case V_STRING:
		return from != V_VOID;
	default:
		return false;
	}
}

//-------------------------------------------------------------------------------------------------
// var stored on stack or as program variable
struct Var
{
	union
	{
		Str* str;
		int Int;
		float Float;
	};
	VAR type;

	Var() : type(V_VOID) {}
	Var(Str* str) : str(str), type(V_STRING) {str->refs++;}
	Var(int Int) : Int(Int), type(V_INT) {}
	Var(float Float) : Float(Float), type(V_FLOAT) {}
	Var(const Var& v)
	{
		type = v.type;
		str = v.str;
		if(type == V_STRING)
			str->refs++;
	}
};
