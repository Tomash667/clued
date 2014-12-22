#include "Pch.h"
#include "Base.h"
#include "Function.h"
#include "Run.h"
#include <conio.h>
#include <iostream>
#include <cmath>
#include <cstdarg>

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
Function::Function(cstring name, VAR return_type, VoidF f, ...) : name(name), return_type(return_type), f(f)
{
	va_list a;
	va_start(a, f);
	while(true)
	{
		VAR type = va_arg(a, VAR);
		if(type == V_VOID)
			break;
		else
			args.push_back(type);
	}
	va_end(a);
}

//=================================================================================================
const Function funcs[] = {
	Function("print", V_VOID, f_print, V_STRING, V_VOID),
	Function("pause", V_VOID, f_pause, V_VOID),
	Function("getstr", V_STRING, f_getstr, V_VOID),
	Function("getint", V_INT, f_getint, V_VOID),
	Function("pow", V_FLOAT, f_pow, V_FLOAT, V_FLOAT, V_VOID),
	Function("getfloat", V_FLOAT, f_getfloat, V_VOID)
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
