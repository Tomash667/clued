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
			{
				byte b = *c;
				if(b >= strs.size())
					throw Format("Invalid cstr index %u.", b);
				stack.push_back(Var(strs[b]));
				++c;
			}
			break;
		case PUSH_VAR:
			{
				byte b = *c;
				if(b >= vars.size())
					throw Format("Invalid var index %u.", b);
				stack.push_back(Var(vars[b]));
				++c;
			}
			break;
		case PUSH_INT:
			stack.push_back(*(int*)c);
			c += 4;
			break;
		case PUSH_FLOAT:
			stack.push_back(*(float*)c);
			c += 4;
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
				byte b = *c;
				if(b >= vars.size())
					throw Format("Invalid var index %u.", b);
				Var& v = vars[b];
				++c;
				if(v.type == V_STRING)
					v.str->Release();
				v = Var(stack.back());
			}
			break;
		case ADD:
			if(stack.size() >= 2)
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_INT)
					v2.Int += v.Int;
				else if(v.type == V_FLOAT)
					v2.Float += v.Float;
				else if(v.type == V_STRING)
				{
					cstring s = Format("%s%s", v2.str->s.c_str(), v.str->s.c_str());
					v.str->Release();
					if(v2.str->refs == 1)
						v2.str->s = s;
					else
					{
						v2.str->Release();
						v2.str = StrPool.Get();
						v2.str->s = s;
						v2.str->refs = 1;
					}
				}
				else
					throw "Invalid operation!";
			}
			else
				throw "Empty stack!";
			break;
		case SUB:
			if(stack.size() >= 2)
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_INT)
					v2.Int -= v.Int;
				else if(v.type == V_FLOAT)
					v2.Float -= v.Float;
				else
					throw "Invalid operation!";
			}
			else
				throw "Empty stack!";
			break;
		case MUL:
			if(stack.size() >= 2)
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_INT)
					v2.Int *= v.Int;
				else if(v.type == V_FLOAT)
					v2.Float *= v.Float;
				else
					throw "Invalid operation!";
			}
			else
				throw "Empty stack!";
			break;
		case DIV:
			if(stack.size() >= 2)
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_INT)
				{
					if(v.Int == 0)
						throw "Division by zero!";
					v2.Int /= v.Int;
				}
				else if(v.type == V_FLOAT)
				{
					if(v.Float == 0.f)
						throw "Division by zero!";
					v2.Float /= v.Float;
				}
				else
					throw "Invalid operation!";
			}
			else
				throw "Empty stack!";
			break;
		case MOD:
			if(stack.size() >= 2)
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_INT)
				{
					if(v.Int == 0)
						throw "Division by zero!";
					v2.Int = v2.Int % v.Int;
				}
				else if(v.type == V_FLOAT)
				{
					if(v.Float == 0.f)
						throw "Division by zero!";
					v2.Float = v2.Float % v.Float;
				}
				else
					throw "Invalid operation!";
			}
			else
				throw "Empty stack!";
			break;
		case NEG:
			if(!stack.empty())
			{
				Var& v = stack.back();
				if(v.type == V_INT)
					v.Int = -v.Int;
				else if(v.type == V_FLOAT)
					v.Float = -v.Float;
				else
					throw "Invalid operation!";
			}
			else
				throw "Empty stack!";
			break;
		case CALL:
			call_func(*c);
			++c;
			break;
		case CAST:
			if(!stack.empty())
			{
				cstring s;
				VAR type = (VAR)*c;
				++c;
				Var& v = stack.back();
				bool invalid = false;
				switch(v.type)
				{
				case V_VOID:
					invalid = true;
					break;
				case V_INT:
					switch(type)
					{
					case V_INT:
						break;
					case V_FLOAT:
						v.Float = (float)v.Int;
						v.type = V_FLOAT;
						break;
					case V_STRING:
						s = Format("%d", v.Int);
						v.str = StrPool.Get();
						v.str->s = s;
						v.str->refs = 1;
						v.type = V_STRING;
						break;
					default:
						invalid = true;
						break;
					}
					break;
				case V_FLOAT:
					switch(type)
					{
					case V_INT:
						v.Int = (int)v.Float;
						v.type = V_INT;
						break;
					case V_FLOAT:
						break;
					case V_STRING:
						s = Format("%g", v.Float);
						v.str = StrPool.Get();
						v.str->s = s;
						v.str->refs = 1;
						v.type = V_STRING;
						break;
					default:
						invalid = true;
						break;
					}
					break;
				case V_STRING:
					if(type != V_STRING)
						invalid = true;
					break;
				default:
					invalid = true;
					break;
				}
				if(invalid)
					throw Format("Invalid cast from %s to %s.", var_name[v.type], var_name[type]);
			}
			else
				throw "Empty stack!";
			break;
		case RET:
			return;
		default:
			throw Format("Unknown op code %d!", op);
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
		if(it->type == V_STRING)
			it->str->Release();
	}
	for(vector<Var>::iterator it = vars.begin(), end = vars.end(); it != end; ++it)
	{
		if(it->type == V_STRING)
			it->str->Release();
	}

	stack.clear();
	vars.clear();
}
