#include "Pch.h"
#include "Base.h"
#include "Run.h"
#include "Op.h"
#include "Function.h"
#include <cmath>

//=================================================================================================
struct Callstack
{
	byte* return_pos;
	int function;
	uint prev_func_vars, args_offset;
};

//=================================================================================================
vector<Var> stack;
vector<Var> vars;
vector<Callstack> callstack;
uint vars_offset, func_vars;
ObjectPool<ScriptFunction> ScriptFunctionPool;
int cmp_result;

//=================================================================================================
void run(byte* code, vector<Str*>& strs, vector<ScriptFunction>& sfuncs)
{
	byte* c = code;
	vars_offset = 0;
	func_vars = 0;

	while(1)
	{
		Op op = (Op)*c;
		++c;
		switch(op)
		{
		case PUSH_VAR:
			{
				byte b = *c + vars_offset;
				if(b >= vars.size())
					throw Format("Invalid var index %u.", b-vars_offset);
				stack.push_back(Var(vars[b]));
				++c;
			}
			break;
		case PUSH_ARG:
			{
				byte b = *c;
				if(callstack.empty() || b >= sfuncs[callstack.back().function].args)
					throw Format("Invalid arg index %u.", b);
				Callstack& cs = callstack.back();
				uint offset = cs.args_offset + b;
				stack.push_back(Var(stack[offset]));
				++c;
			}
			break;
		case PUSH_CSTR:
			{
				byte b = *c;
				if(b >= strs.size())
					throw Format("Invalid cstr index %u.", b);
				stack.push_back(Var(strs[b]));
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
			if (!stack.empty())
			{
				if (stack.back().type == V_STRING)
					stack.back().str->Release();
				stack.pop_back();
			}
			else
				throw "Empty stack.";
			break;
		case SET_VARS:
			func_vars = *c;
			vars.resize(func_vars + vars_offset);
			++c;
			break;
		case SET_VAR:
			{
				byte b = *c + vars_offset;
				if(b >= vars.size())
					throw Format("Invalid var index %u.", b + vars_offset);
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
					v2.Float = fmod(v2.Float, v.Float);
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
		case CALLF:
			{
				byte b = *c;
				++c;
				if(b >= sfuncs.size())
					throw Format("Invalid script function index %u.", b);
				Callstack& cs = Add1(callstack);
				cs.function = b;
				cs.return_pos = c;
				cs.prev_func_vars = func_vars;
				cs.args_offset = stack.size() - sfuncs[b].args;
				c = code + sfuncs[b].pos;
				vars_offset = vars.size();
				func_vars = 0;
			}
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
				case V_BOOL:
					switch (type)
					{
					case V_BOOL:
						break;
					case V_INT:
						v.Int = (v.Bool ? 1 : 0);
						v.type = V_INT;
						break;
					case V_FLOAT:
						v.Float = (v.Bool ? 1.0f : 0.f);
						v.type = V_FLOAT;
						break;
					case V_STRING:
						s = (v.Bool ? "1" : "0");
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
				case V_INT:
					switch(type)
					{
					case V_BOOL:
						v.Bool = (v.Int != 0);
						v.type = V_BOOL;
						break;
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
					case V_BOOL:
						v.Bool = (v.Float != 0.f);
						v.type = V_BOOL;
						break;
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
			if(callstack.empty())
				return;
			else
			{
				Callstack& call = callstack.back();
				ScriptFunction& f = sfuncs[call.function];
				Var result;
				if(f.have_result)
				{
					result = stack.back();
					stack.pop_back();
				}
				for(int i = 0; i < f.args; ++i)
				{
					if(stack.back().type == V_STRING)
						stack.back().str->Release();
					stack.pop_back();
				}
				if(f.have_result)
					stack.push_back(result);
				for(uint i = 0; i < func_vars; ++i)
				{
					if(vars.back().type == V_STRING)
						vars.back().str->Release();
					vars.pop_back();
				}
				func_vars = call.prev_func_vars;
				vars_offset -= func_vars;
				c = call.return_pos;
				callstack.pop_back();
			}
			break;
		case CMP:
			if(stack.size() >= 2)
			{
				Var b = stack.back();
				stack.pop_back();
				Var& a = stack.back();
				switch(a.type)
				{
				case V_BOOL:
					if (a.Bool == b.Bool)
						cmp_result = 0;
					else
						cmp_result = 1;
					break;
				case V_INT:
					if(a.Int > b.Int)
						cmp_result = 1;
					else if(a.Int == b.Int)
						cmp_result = 0;
					else
						cmp_result = -1;
					break;
				case V_FLOAT:
					if(a.Float > b.Float)
						cmp_result = 1;
					else if(a.Float == b.Float)
						cmp_result = 0;
					else
						cmp_result = -1;
					break;
				case V_STRING:
					if(a.str->s == b.str->s)
						cmp_result = 0;
					else
						cmp_result = 1;
					a.str->Release();
					b.str->Release();
					break;
				default:
					cmp_result = 1;
					break;
				}
				stack.pop_back();
			}
			else
				throw "Empty stack!";
			break;
		case TEST:
			if (!stack.empty())
			{
				if (stack.back().Bool)
					cmp_result = 0;
				else
					cmp_result = 1;
				stack.pop_back();
			}
			else
				throw "Empty stack!";
			break;
		case JMP:
			{
				short dist = *(short*)c;
				c += dist;
			}
			break;
		case JE:
			{
				short dist = *(short*)c;
				if(cmp_result == 0)
					c += dist;
				else
					c += 2;
			}
			break;
		case JNE:
			{
				short dist = *(short*)c;
				if(cmp_result != 0)
					c += dist;
				else
					c += 2;
			}
			break;
		case JG:
			{
				short dist = *(short*)c;
				if(cmp_result > 0)
					c += dist;
				else
					c += 2;
			}
			break;
		case JGE:
			{
				short dist = *(short*)c;
				if(cmp_result >= 0)
					c += dist;
				else
					c += 2;
			}
			break;
		case JL:
			{
				short dist = *(short*)c;
				if(cmp_result == 0)
					c += dist;
				else
					c += 2;
			}
			break;
		case JLE:
			{
				short dist = *(short*)c;
				if(cmp_result == 0)
					c += dist;
				else
					c += 2;
			}
			break;
		case JES:
			stack.push_back(Var(cmp_result == 0));
			break;
		case JNES:
			stack.push_back(Var(cmp_result != 0));
			break;
		case JGS:
			stack.push_back(Var(cmp_result > 0));
			break;
		case JGES:
			stack.push_back(Var(cmp_result >= 0));
			break;
		case JLS:
			stack.push_back(Var(cmp_result < 0));
			break;
		case JLES:
			stack.push_back(Var(cmp_result <= 0));
			break;
		default:
			throw Format("Unknown op code %d!", op);
		}
	}
}

//=================================================================================================
void try_run(byte* code, vector<Str*>& strs, vector<ScriptFunction>& sfuncs)
{
	try
	{
		run(code, strs, sfuncs);
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
