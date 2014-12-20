#include "Pch.h"
#include "Base.h"
#include "Function.h"
#include "Run.h"
#include <conio.h>
#include <iostream>
#include <cmath>

//=================================================================================================
void f_print()
{
	if(stack.empty())
		throw "Empty stack!";
	Var& v = stack.back();
	printf(v.str->s.c_str());
	v.str->Release();
	stack.pop_back();
}

//=================================================================================================
void f_pause()
{
	_getch();
}

//=================================================================================================
void f_getstr()
{
	Str* s = StrPool.Get();
	std::cin >> s->s;
	s->refs = 1;
	stack.push_back(Var(s));
}

//=================================================================================================
void f_getint()
{
	int a;
	std::cin >> a;
	stack.push_back(Var(a));
}

//=================================================================================================
void f_pow()
{
	if(stack.size() < 2)
		throw "Empty stack!";
	float a = stack.back().Float;
	stack.pop_back();
	float b = stack.back().Float;
	stack.back().Float = pow(a, b);
}

//=================================================================================================
void f_getfloat()
{
	float a;
	std::cin >> a;
	stack.push_back(Var(a));
}

//=================================================================================================
#define ARGS0() 0, {V_VOID, V_VOID}
#define ARGS1(a1) 1, {a1, V_VOID}
#define ARGS2(a1, a2) 2, {a1, a2}

//=================================================================================================
const Function funcs[] = {
	"print", V_VOID, ARGS1(V_STRING), f_print,
	"pause", V_VOID, ARGS0(), f_pause,
	"getstr", V_STRING, ARGS0(), f_getstr,
	"getint", V_INT, ARGS0(), f_getint,
	"pow", V_INT, ARGS2(V_FLOAT, V_FLOAT), f_pow,
	"getfloat", V_FLOAT, ARGS0(), f_getfloat
};
const int n_funcs = countof(funcs);

//=================================================================================================
void pause()
{
	_getch();
}

//=================================================================================================
void call_func(int id)
{
	if(id >= n_funcs)
		throw Format("Invalid function index %d.", id);
	funcs[id].f();
}
