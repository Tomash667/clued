#pragma once

//-------------------------------------------------------------------------------------------------
#include "Str.h"

//-------------------------------------------------------------------------------------------------
// variable type
enum VAR
{
	V_VOID,
	V_BOOL,
	V_INT,
	V_FLOAT,
	V_CHAR,
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
		return false;
	case V_INT:
		if(from == V_FLOAT || from == V_CHAR)
			return true;
		return false;
	case V_FLOAT:
		if(from == V_INT)
			return true;
		return false;
	case V_STRING:
		return from != V_VOID;
	case V_CHAR:
		if(from == V_INT)
			return true;
		return false;
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
		bool Bool;
		int Int;
		float Float;
		char Char;
		Str* str;
	};
	VAR type;

	Var() : type(V_VOID) {}
	Var(bool Bool) : Bool(Bool), type(V_BOOL) {}
	Var(int Int) : Int(Int), type(V_INT) {}
	Var(float Float) : Float(Float), type(V_FLOAT) {}
	Var(char Char) : Char(Char), type(V_CHAR) {}
	Var(Str* str) : str(str), type(V_STRING) { str->refs++; }
	Var(const Var& v)
	{
		type = v.type;
		str = v.str;
		Release();
	}

	inline void Release()
	{
		if(type == V_STRING)
			str->Release();
	}
};
