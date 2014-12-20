#include "Pch.h"
#include <conio.h>
#include "Base.h"
#include <iostream>

enum Op : byte
{
	PUSH_CSTR,
	PUSH_VAR,
	POP,
	SET_VARS,
	SET_VAR,
	ADD,
	SUB,
	MUL,
	DIV,
	CALL,
	RET
};

int GetPriority(int op)
{
	if(op == MUL || op == DIV)
		return 2;
	else
		return 1;
}

char OpChar(int op)
{
	switch(op)
	{
	case ADD:
		return '+';
	case SUB:
		return '-';
	case MUL:
		return '*';
	case DIV:
		return '/';
	default:
		return '?';
	}
}

enum VAR
{
	V_VOID,
	V_INT,
	V_CSTR,
	V_STR
};

cstring var_name[] = {
	"void",
	"int",
	"cstr",
	"str"
};

bool CanCast(VAR to, VAR from)
{
	if(from == to)
		return true;

	switch(to)
	{
	case V_VOID:
	case V_INT:
		return false;
	case V_CSTR:
		return from == V_STR || from == V_INT;
	case V_STR:
		return from == V_CSTR || from == V_INT;
	default:
		return false;
	}
}

VAR CanOp(VAR a, Op op, VAR b)
{
	switch(op)
	{
	case ADD:
		if(a == V_INT && b == V_INT)
			return V_INT;
		else if((a == V_STR || a == V_CSTR || a == V_INT) && (b == V_STR || b == V_CSTR || b == V_INT))
			return V_STR;
		else
			return V_VOID;
	case SUB:
	case DIV:
	case MUL:
		if(a == V_INT && b == V_INT)
			return V_INT;
		else
			return V_VOID;
	default:
		return V_VOID;
	}
}

struct String
{
	string s;
	int refs;

	inline void Release()
	{
		--refs;
		if(refs == 0)
			delete this;
	}
};

struct Var
{
	union
	{
		cstring cstr;
		String* str;
		int Int;
	};
	VAR type;

	Var() : type(V_VOID) {}
	Var(cstring cstr) : cstr(cstr), type(V_CSTR) {}
	Var(String* str) : str(str), type(V_STR) {str->refs++;}
	Var(int Int) : Int(Int), type(V_INT) {}
	Var(const Var& v)
	{
		type = v.type;
		cstr = v.cstr;
		if(type == V_STR)
			str->refs++;
	}
};

enum Func : byte
{
	F_PRINT,
	F_PAUSE,
	F_GETSTR,
	F_GETINT
};

typedef void (*VoidF)();

#define MAX_ARGS 2

struct Function
{
	cstring name;
	VAR return_type;
	int arg_count;
	VAR args[MAX_ARGS];
	VoidF f;
};

vector<string> strs;
vector<Var> stack;

void f_print()
{
	Var& v = stack.back();
	if(v.type == V_STR)
	{
		printf(v.str->s.c_str());
		v.str->Release();
	}
	else
		printf(v.cstr);
	stack.pop_back();
}

void f_pause()
{
	_getch();
}

void f_getstr()
{
	String* s = new String;
	std::cin >> s->s;
	s->refs = 1;
	stack.push_back(Var(s));
}

void f_getint()
{
	int a;
	std::cin >> a;
	stack.push_back(Var(a));
}

#define ARGS0() 0, {V_VOID, V_VOID}
#define ARGS1(a1) 1, {a1, V_VOID}
#define ARGS2(a1, a2) 2, {a1, a2}

Function funcs[] = {
	"print", V_VOID, ARGS1(V_CSTR), f_print,
	"pause", V_VOID, ARGS0(), f_pause,
	"getstr", V_STR, ARGS0(), f_getstr,
	"getint", V_INT, ARGS0(), f_getint
};

void run(byte* code)
{
	vector<Var> vars;
	byte* c = code;
	while(1)
	{
		Op op = (Op)*c;
		++c;
		switch(op)
		{
		case PUSH_CSTR:
			stack.push_back(Var(strs[*c].c_str()));
			++c;
			break;
		case PUSH_VAR:
			stack.push_back(Var(vars[*c]));
			++c;
			break;
		case POP:
			if(stack.back().type == V_STR)
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
				if(v.type == V_STR)
					v.str->Release();
				v = Var(stack.back());
			}
			break;
		case ADD:
			{
				Var v = stack.back();
				stack.pop_back();
				Var& v2 = stack.back();
				if(v.type == V_STR || v.type == V_CSTR || v2.type == V_STR || v2.type == V_CSTR)
				{
					cstring s1, s2;
					if(v.type == V_STR)
						s1 = v.str->s.c_str();
					else if(v.type == V_CSTR)
						s1 = v.cstr;
					else
						s1 = Format("%d", v.Int);
					if(v2.type == V_STR)
						s2 = v2.str->s.c_str();
					else if(v2.type == V_CSTR)
						s2 = v2.cstr;
					else
						s2 = Format("%d", v.Int);
					cstring s = Format("%s%s", s2, s1);
					if(v.type == V_STR)
						v.str->Release();
					if(v2.type == V_STR)
					{
						if(v2.str->refs == 1)
							v2.str->s = s;
						else
						{
							v2.str->Release();
							v2.str = new String;
							v2.str->refs = 1;
							v2.str->s = s;
						}
					}
					else
					{
						v2.type = V_STR;
						v2.str = new String;
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
		case CALL:
			funcs[*c].f();
			++c;
			break;
		case RET:
			return;
		}
	}
}

vector<byte> bcode;
Tokenizer t;

struct ParseVar
{
	string name;
	VAR type;
	int line;
};

struct Node
{
	enum NodeOp
	{
		N_CALL,
		N_OP,
		N_VAR,
		N_CSTR,
		N_SET
	} op;
	int id;
	vector<Node*> nodes;
	VAR return_type;
};

vector<ParseVar> vars;
vector<Node*> top_nodes;
vector<Node*> node_out, node_stack;

Node* parse_expr(char funcend)
{
	// expr -> expr [op expr]
	if(t.IsSymbol(funcend))
		return NULL;

	Node* node = new Node;

	while(true)
	{
		if(t.IsKeywordGroup(1))
		{
			// func
			int fid = t.GetKeywordId();
			const Function& f = funcs[fid];
			Node* fnode = new Node;
			fnode->op = Node::N_CALL;
			fnode->id = fid;
			fnode->return_type = f.return_type;
			// (
			t.Next();
			t.AssertSymbol('(');
			t.Next();
			// args
			if(f.arg_count > 0)
			{
				for(int i=0; i<f.arg_count; ++i)
				{
					Node* result = parse_expr(i < f.arg_count-1 ? ',' : ')');
					if(!result)
						t.Throw(Format("Empty statement as %d arg of function %s.", i+1, f.name));
					if(!CanCast(f.args[i], result->return_type))
						t.Throw(Format("Can't cast from %s to %s for arg %d in function call %s.", var_name[result->return_type], var_name[f.args[i]], i+1, f.name));
					fnode->nodes.push_back(result);
					t.Next();
				}
			}
			else
			{
				t.AssertSymbol(')');
				t.Next();
			}
			node->nodes.push_back(fnode);
		}
		else if(t.IsItem())
		{
			// var
			const string& s = t.MustGetItem();
			int index = 0;
			bool ok = false;
			for(vector<ParseVar>::iterator it = vars.begin(), end = vars.end(); it != end; ++it, ++index)
			{
				if(s == it->name)
				{
					ok = true;
					break;
				}
			}
			if(!ok)
				t.Unexpected();
			ParseVar& v = vars[index];
			Node* vnode = new Node;
			vnode->op = Node::N_VAR;
			vnode->id = index;
			vnode->return_type = v.type;
			node->nodes.push_back(vnode);
			t.Next();
		}
		else if(t.IsString())
		{
			int si = strs.size();
			string& s = Add1(strs);
			Unescape(t.GetString(), s);
			t.Next();
			Node* csnode = new Node;
			csnode->op = Node::N_CSTR;
			csnode->id = si;
			csnode->return_type = V_CSTR;
			node->nodes.push_back(csnode);
		}
		else
			t.Unexpected();

		if(t.IsSymbol(funcend))
			break;
		else if(t.IsSymbol('='))
		{
			if(node->nodes.size() > 1 || node->nodes[0]->op != Node::N_VAR)
				t.Throw("Can't assign becouse lvalue is not var.");
			node->op = Node::N_SET;
			node->id = node->nodes[0]->id;
			node->return_type = node->nodes[0]->return_type;
			delete node->nodes[0];
			node->nodes.clear();
			t.Next();
			Node* result = parse_expr(funcend);
			if(!result)
				t.Throw(Format("Can't assign empty expression to var %s '%s'.", var_name[node->return_type], vars[node->id].name.c_str()));
			node->nodes.push_back(result);
			return node;
		}
		else if(t.IsSymbol('+') || t.IsSymbol('-') || t.IsSymbol('*') || t.IsSymbol('/'))
		{
			Node* onode = new Node;
			onode->op = Node::N_OP;
			switch(t.GetSymbol())
			{
			case '+':
				onode->id = ADD;
				break;
			case '-':
				onode->id = SUB;
				break;
			case '*':
				onode->id = MUL;
				break;
			case '/':
				onode->id = DIV;
				break;
			}
			node->nodes.push_back(onode);
			t.Next();
		}
		else
			t.Unexpected();
	}

	// if 1 element then simply return it
	if(node->nodes.size() == 1)
	{
		Node* result = node->nodes[0];
		delete node;
		return result;
	}

	// convert infix to RPN
	node_out.clear();
	node_stack.clear();
	for(vector<Node*>::iterator it = node->nodes.begin(), end = node->nodes.end(); it != end; ++it)
	{
		Node* n = *it;
		if(n->op == Node::N_OP)
		{
			if(!node_stack.empty())
			{
				int prio = GetPriority(n->id);
				while(!node_stack.empty() && prio <= GetPriority(node_stack.back()->id))
				{
					node_out.push_back(node_stack.back());
					node_stack.pop_back();
				}
			}
			node_stack.push_back(n);
		}
		else
			node_out.push_back(n);
	}
	while(!node_stack.empty())
	{
		node_out.push_back(node_stack.back());
		node_stack.pop_back();
	}

	// conver RPN to tree
	node->nodes.clear();
	for(vector<Node*>::iterator it = node_out.begin(), end = node_out.end(); it != end; ++it)
	{
		Node* n = *it;
		if(n->op == Node::N_OP)
		{
			Node* b = node->nodes.back();
			node->nodes.pop_back();
			Node* a = node->nodes.back();
			node->nodes.pop_back();
			VAR result = CanOp(a->return_type, (Op)n->id, b->return_type);
			if(result == V_VOID)
				t.Throw(Format("Invalid operation %s %c %s.", var_name[a->return_type], OpChar(n->id), var_name[b->return_type]));
			n->nodes.push_back(a);
			n->nodes.push_back(b);
			n->return_type = result;
			node->nodes.push_back(n);
		}
		else
			node->nodes.push_back(n);
	}

	// return result
	Node* result = node->nodes[0];
	delete node;
	return result;
}

void parse_exprl()
{
	// exprl -> expr ;
	Node* result = parse_expr(';');
	t.AssertSymbol(';');
	top_nodes.push_back(result);
}

void parse_vard()
{
	// vard -> var_type name [= expr] ;
	VAR type = (VAR)t.GetKeywordId();
	int line = t.GetLine()+1;

	t.Next();
	const string& s = t.MustGetItem();
	// check is unique
	for(vector<ParseVar>::iterator it = vars.begin(), end = vars.end(); it != end; ++it)
	{
		if(it->name == s)
			t.Throw(Format("Variable '%s' already declared at line %d with type 'string'.", s.c_str(), it->line));
	}
	int vi = vars.size();
	ParseVar& v = Add1(vars);
	v.name = s;
	v.type = type;
	v.line = line;

	t.Next();
	if(t.IsSymbol('='))
	{
		t.Next();
		Node* result = parse_expr(';');
		if(!result)
			t.Throw(Format("Empty assign expression for var %s '%s'.", var_name[v.type], v.name.c_str()));
		if(!CanCast(type, result->return_type))
			t.Throw(Format("Can't assign to var %s '%s' value of type '%s'.", var_name[v.type], v.name.c_str(), var_name[result->return_type]));
		Node* node = new Node;
		node->op = Node::N_SET;
		node->id = vi;
		node->return_type = v.type;
		node->nodes.push_back(result);
		top_nodes.push_back(node);
	}
	t.AssertSymbol(';');
}

void parse_node(Node* node, bool top)
{
	if(node->op == Node::N_CALL)
	{
		// calls push args from right to left
		for(vector<Node*>::reverse_iterator it = node->nodes.rbegin(), end = node->nodes.rend(); it != end; ++it)
			parse_node(*it, false);
	}
	else
	{
		// left to right
		for(vector<Node*>::iterator it = node->nodes.begin(), end = node->nodes.end(); it != end; ++it)
			parse_node(*it, false);
	}
	switch(node->op)
	{
	case Node::N_CALL:
		bcode.push_back(CALL);
		bcode.push_back(node->id);
		break;
	case Node::N_CSTR:
		bcode.push_back(PUSH_CSTR);
		bcode.push_back(node->id);
		break;
	case Node::N_OP:
		bcode.push_back(node->id);
		break;
	case Node::N_VAR:
		bcode.push_back(PUSH_VAR);
		bcode.push_back(node->id);
		break;
	case Node::N_SET:
		bcode.push_back(SET_VAR);
		bcode.push_back(node->id);
		break;
	default:
		assert(0);
		break;
	}
	if(top && node->return_type != V_VOID)
	{
		// pop unused return value
		bcode.push_back(POP);
	}
}

void parse(cstring file)
{
	//t.AddKeyword("return", 0);
	for(int i=0, count=countof(funcs); i<count; ++i)
		t.AddKeyword(funcs[i].name, i, 1);
	t.AddKeyword("int", V_INT, 2);
	t.AddKeyword("string", V_STR, 2);
	t.FromFile(file);

	while(true)
	{
		t.Next();
		if(t.IsEof())
			break;

		if(t.IsKeywordGroup(2))
			parse_vard();
		else
			parse_exprl();
	}

	// create byte code from expression tree
	if(vars.size() > 0)
	{
		bcode.push_back(SET_VARS);
		bcode.push_back(vars.size());
	}
	for(vector<Node*>::iterator it = top_nodes.begin(), end = top_nodes.end(); it != end; ++it)
		parse_node(*it, true);
	bcode.push_back(RET);
}

int main()
{
	try
	{
		parse("3.txt");
		run(&bcode[0]);
	}
	catch(cstring err)
	{
		printf("CLUED PARSE ERROR: %s\n", err);
		_getch();
	}
	return 0;
}
