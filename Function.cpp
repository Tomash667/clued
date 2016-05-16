#include "Pch.h"
#include "Base.h"
#include "Function.h"
#include "Run.h"
#include <conio.h>
#include <iostream>
#include <cmath>
#include <cstdarg>

//=================================================================================================
void f_print(Str* s)
{
	printf(s->s.c_str());
}

//=================================================================================================
void f_pause()
{
	(void)_getch();
}

//=================================================================================================
Str* f_getstr()
{
	Str* s = StrPool.Get();
	std::cin >> s->s;
	s->refs = 1;
	return s;
}

//=================================================================================================
int f_getint()
{
	int a;
	std::cin >> a;
	return a;
}

//=================================================================================================
float f_pow(float a, float b)
{
	return pow(a, b);
}

//=================================================================================================
float f_getfloat()
{
	float a;
	std::cin >> a;
	return a;
}

//=================================================================================================
const Function funcs[] = {
	"print", V_VOID, { V_STRING }, f_print,
	"pause", V_VOID, {}, f_pause,
	"getstr", V_STRING, {}, f_getstr,
	"getint", V_INT, {}, f_getint,
	"pow", V_FLOAT, { V_FLOAT, V_FLOAT }, f_pow,
	"getfloat", V_FLOAT, {}, f_getfloat,
};
const int n_funcs = countof(funcs);

//=================================================================================================
void pause()
{
	(void)_getch();
}

vector<Str*> str_to_pop;

//=================================================================================================
void call_func(int id)
{
	if(id >= n_funcs)
		throw Format("Invalid function index %d.", id);

	// push args
	const Function& f = funcs[id];
	for (VAR v : f.args)
	{
		if (stack.empty())
			throw Format("Empty stack for function %s.", f.name);
		Var& v2 = stack.back();
		if (v2.type != v)
			throw "Argument missmatch.";
		int a = v2.Int;
		__asm
		{
			push a;
		}
		if (v2.type == V_STRING)
			str_to_pop.push_back(v2.str);
		stack.pop_back();
	}

	void* fptr = f.f;
	int a = f.args.size() * 4;

	// call
	if (f.return_type != V_FLOAT)
	{
		int b;

		__asm
		{
			call fptr;
			mov b, eax;
			add esp, a;
		}

		// result
		if (f.return_type != V_VOID)
		{
			Var& v = Add1(stack);
			v.type = f.return_type;
			v.Int = b;
		}
	}
	else
	{
		float b;

		__asm
		{
			call fptr;
			fstp b;
			add esp, a;
		}

		Var& v = Add1(stack);
		v.type = V_FLOAT;
		v.Float = b;
	}

	// pop strings
	for (Str* s : str_to_pop)
	{
		s->Release();
	}
}
