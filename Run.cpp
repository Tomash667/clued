#include "Pch.h"
#include "Base.h"
#include "Run.h"
#include "Op.h"
#include "Function.h"

//=================================================================================================
vector<Var> stack;
vector<Var> vars;

//=================================================================================================
void run(byte* code, vector<Str*>& strs)
{
	byte* c = code;
	while(1)
	{
		Op op = (Op)*c;
		++c;
		switch(op)
		{
		case PUSH_CSTR:
			stack.push_back(Var(strs[*c]));
			++c;
			break;
		case PUSH_VAR:
			stack.push_back(Var(vars[*c]));
			++c;
			break;
		case PUSH_INT:
			{
				int a = *(int*)c;
				c += 4;
				stack.push_back(a);
			}
			break;
		case POP:
			if(stack.back().type == V_STRING)
				stack.back().str->Release();
			stack.pop_back();
			break;
		case SET_VARS:
			vars.resize(*c);
			++c;
			break;
		case SET_VAR:
			{
				Var& v = vars[*c];
				++c;
				if(v.type == V_STRING)
					v.str->Release();
				v = Var(stack.back());
			}
			break;
		case ADD:
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_STRING || v2.type == V_STRING)
				{
					cstring s1, s2;
					if(v.type == V_STRING)
						s1 = v.str->s.c_str();
					else
						s1 = Format("%d", v.Int);
					if(v2.type == V_STRING)
						s2 = v2.str->s.c_str();
					else
						s2 = Format("%d", v2.Int);
					cstring s = Format("%s%s", s2, s1);
					if(v.type == V_STRING)
						v.str->Release();
					if(v2.type == V_STRING)
					{
						if(v2.str->refs == 1)
							v2.str->s = s;
						else
						{
							v2.str->Release();
							v2.str = StrPool.Get();
							v2.str->refs = 1;
							v2.str->s = s;
						}
					}
					else
					{
						v2.type = V_STRING;
						v2.str = StrPool.Get();
						v2.str->refs = 1;
						v2.str->s = s;
					}
				}
				else
				{
					// adding ints
					v2.Int += v.Int;
				}
			}
			break;
		case SUB:
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				v2.Int -= v.Int;
			}
			break;
		case MUL:
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				v2.Int *= v.Int;
			}
			break;
		case DIV:
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.Int == 0)
					throw "Division by zero!";
				v2.Int /= v.Int;
			}
			break;
		case NEG:
			stack.back().Int = -stack.back().Int;
			break;
		case CALL:
			funcs[*c].f();
			++c;
			break;
		case TO_STRING:
			{
				Var& v = stack.back();
				if(v.type == V_INT)
				{
					int Int = v.Int;
					v.str = StrPool.Get();
					v.str->s = Format("%d", Int);
					v.str->refs = 1;
					v.type = V_STRING;
				}
			}
			break;
		case RET:
			return;
		}
	}
}

//=================================================================================================
void try_run(byte* code, vector<Str*>& strs)
{
	try
	{
		run(code, strs);
		if(!stack.empty())
			throw "Stack not empty!";
	}
	catch(cstring err)
	{
		printf("CLUED RUNTIME ERROR: %s", err);
		pause();
	}

	for(vector<Var>::iterator it = stack.begin(), end = stack.end(); it != end; ++it)
	{
		if(it->type == V_INT)
			it->str->Release();
	}
	for(vector<Var>::iterator it = vars.begin(), end = vars.end(); it != end; ++it)
	{
		if(it->type == V_INT)
			it->str->Release();
	}

	stack.clear();
	vars.clear();
}
