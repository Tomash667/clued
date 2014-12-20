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
	int a = stack.back().Int;
	stack.pop_back();
	int b = stack.back().Int;
	stack.back().Int = (int)pow((float)a, b);
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
	"pow", V_INT, ARGS2(V_INT, V_INT), f_pow
};
const int n_funcs = countof(funcs);

//=================================================================================================
void pause()
{
	_getch();
}
