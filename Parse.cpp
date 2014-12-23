#include "Pch.h"
#include "Base.h"
#include "Parse.h"
#include "Var.h"
#include "Function.h"
#include "Op.h"
#include "ScriptFunction.h"

//=================================================================================================
enum Keyword
{
	K_RETURN,
	K_IF,
	K_ELSE
};

//=================================================================================================
VAR CanOp(VAR a, Op op, VAR b, bool& return_bool)
{
	switch(op)
	{
	case ADD:
		if(a == b)
		{
			if(a != V_VOID && a != V_BOOL)
				return a;
			else
				return V_VOID;
		}
		else if(a == V_STRING || b == V_STRING)
			return V_STRING;
		else if((a == V_FLOAT || b == V_FLOAT) && (a == V_INT || b == V_INT))
			return V_FLOAT;
		else
			return V_VOID;
	case SUB:
	case DIV:
	case MUL:
	case MOD:
		if(a == b)
		{
			if(a == V_INT || a == V_FLOAT)
				return a;
			else
				return V_VOID;
		}
		else if((a == V_FLOAT || b == V_FLOAT) && (a == V_INT || b == V_INT))
			return V_FLOAT;
		else
			return V_VOID;
	case JE:
	case JNE:
		return_bool = true;
		if(a == b)
			return a;
		else if((a == V_FLOAT || b == V_FLOAT) && (a == V_INT || b == V_INT))
			return V_FLOAT;
		else
			return V_VOID;
	case JG:
	case JGE:
	case JL:
	case JLE:
		return_bool = true;
		if(a == b)
		{
			if(a == V_INT || a == V_FLOAT)
				return a;
			else
				return V_VOID;
		}
		else if((a == V_FLOAT || b == V_FLOAT) && (a == V_INT || b == V_INT))
			return V_FLOAT;
		else
			return V_VOID;
	default:
		return V_VOID;
	}
}

//=================================================================================================
static vector<Str*>* strs;
static vector<byte>* code;
static Tokenizer t;

//=================================================================================================
struct ParseVar
{
	string name;
	VAR type;
	int line;
};

//=================================================================================================
struct Node
{
	enum NodeOp
	{
		N_CALL,
		N_CALLF,
		N_OP,
		N_VAR,
		N_ARG,
		N_SET,
		N_INT,
		N_FLOAT,
		N_CSTR,
		N_CAST,
		N_RETURN,
		N_IF,
		N_BLOCK
	} op;
	union
	{
		int id;
		float fl;
	};
	vector<Node*> nodes;
	VAR return_type;
};

//=================================================================================================
struct ParseFunction
{
	string name;
	VAR return_type;
	vector<ParseVar> args, vars;
	vector<Node*> nodes;
	vector<VAR> arg_types;
};

//=================================================================================================
static ObjectPool<Node> NodePool;
static ObjectPool<ParseFunction> FunctionPool;
static vector<Node*> node_out, node_stack;
static vector<ParseFunction*> functions;
static ParseFunction* top_function;

//=================================================================================================
static Node* parse_expr(char funcend, char funcend2=0);
static void parse_block(vector<Node*>& cont, char funcend);

//=================================================================================================
static void parse_args(Node* fnode, const string& name, const vector<VAR>& args)
{
	// (
	t.Next();
	t.AssertSymbol('(');
	t.Next();
	// args
	if(!args.empty())
	{
		int index = 1;
		for(vector<VAR>::const_iterator it = args.begin(), end = args.end(); it != end; ++it, ++index)
		{
			VAR type = *it;
			Node* result = parse_expr(it+1 != end ? ',' : ')');
			if(!result)
				t.Throw(Format("Empty statement as %d arg of function %s.", index, name.c_str()));
			if(!CanCast(type, result->return_type))
				t.Throw(Format("Can't cast from %s to %s for arg %d in function call %s.", var_name[result->return_type], var_name[type], index, name.c_str()));
			if(type != result->return_type)
			{
				Node* cast = NodePool.Get();
				cast->op = Node::N_CAST;
				cast->return_type = type;
				cast->nodes.push_back(result);
				fnode->nodes.push_back(cast);
			}
			else
				fnode->nodes.push_back(result);
			t.Next();
		}
	}
	else
	{
		t.AssertSymbol(')');
		t.Next();
	}
}

//=================================================================================================
static Node* parse_statement()
{
	// statement ->
	//		var
	//		func
	//		const
	if(t.IsKeywordGroup(1))
	{
		// func
		int fid = t.GetKeywordId();
		const Function& f = funcs[fid];
		Node* fnode = NodePool.Get();
		fnode->op = Node::N_CALL;
		fnode->id = fid;
		fnode->return_type = f.return_type;
		parse_args(fnode, f.name, f.args);
		return fnode;
	}
	else if(t.IsItem())
	{
		// var or script function
		const string& s = t.MustGetItem();
		int index = 0;
		for(vector<ParseVar>::iterator it = top_function->vars.begin(), end = top_function->vars.end(); it != end; ++it, ++index)
		{
			if(s == it->name)
			{
				ParseVar& v = top_function->vars[index];
				Node* vnode = NodePool.Get();
				vnode->op = Node::N_VAR;
				vnode->id = index;
				vnode->return_type = v.type;
				t.Next();
				return vnode;
			}
		}
		index = 0;
		for(vector<ParseFunction*>::iterator it = functions.begin(), end = functions.end(); it != end; ++it, ++index)
		{
			if(s == (*it)->name)
			{
				ParseFunction& f = **it;
				Node* fnode = NodePool.Get();
				fnode->op = Node::N_CALLF;
				fnode->id = index - 1;
				fnode->return_type = f.return_type;
				parse_args(fnode, f.name, f.arg_types);
				return fnode;
			}
		}
		index = 0;
		for(vector<ParseVar>::iterator it = top_function->args.begin(), end = top_function->args.end(); it != end; ++it, ++index)
		{
			if(s == it->name)
			{
				ParseVar& v = top_function->args[index];
				Node* vnode = NodePool.Get();
				vnode->op = Node::N_ARG;
				vnode->id = index;
				vnode->return_type = v.type;
				t.Next();
				return vnode;
			}
		}
	}
	else if(t.IsString())
	{
		// string literal
		int si = strs->size();
		Str* s = StrPool.Get();
		s->refs = 1;
		Unescape(t.GetString(), s->s);
		strs->push_back(s);
		t.Next();
		Node* csnode = NodePool.Get();
		csnode->op = Node::N_CSTR;
		csnode->id = si;
		csnode->return_type = V_STRING;
		return csnode;
	}
	else if(t.IsInt())
	{
		// int literal
		Node* inode = NodePool.Get();
		inode->op = Node::N_INT;
		inode->id = t.GetInt();
		inode->return_type = V_INT;
		t.Next();
		return inode;
	}
	else if(t.IsFloat())
	{
		// float literal
		Node* fnode = NodePool.Get();
		fnode->op = Node::N_FLOAT;
		fnode->fl = t.GetFloat();
		fnode->return_type = V_FLOAT;
		t.Next();
		return fnode;
	}
	else if(t.IsSymbol('-'))
	{
		// -a
		t.Next();
		Node* result = parse_statement();
		if(result->return_type == V_INT)
		{
			Node* node = NodePool.Get();
			node->op = Node::N_OP;
			node->id = NEG;
			node->return_type = V_INT;
			node->nodes.push_back(result);
			return node;
		}
	}
	else if(t.IsSymbol('+'))
	{
		// +a
		Node* result = parse_statement();
		if(result->return_type == V_INT)
			return result;
	}

	t.Unexpected();
	assert(0);
	return NULL;
}

//=================================================================================================
static Node* parse_expr(char funcend, char funcend2)
{
	// expr -> expr [op expr]
	if(t.IsSymbol(funcend) || t.IsSymbol(funcend2))
		return NULL;

	Node* node = NodePool.Get();

	while(true)
	{
		Node* result = parse_statement();
		node->nodes.push_back(result);

		if(!t.IsSymbol())
			t.Unexpected();

		if(t.IsSymbol(funcend) || t.IsSymbol(funcend2))
			break;

		switch(t.GetSymbol())
		{
		case '=':
			t.Next();
			if(!t.IsSymbol('='))
			{
				// assingment
				if(node->nodes.size() > 1 || node->nodes[0]->op != Node::N_VAR)
					t.Throw("Can't assign becouse lvalue is not var.");
				node->op = Node::N_SET;
				node->id = node->nodes[0]->id;
				node->return_type = node->nodes[0]->return_type;
				node->nodes[0]->nodes.clear();
				NodePool.Free(node->nodes[0]);
				node->nodes.clear();
				Node* result = parse_expr(funcend);
				if(!result)
					t.Throw(Format("Can't assign empty expression to var %s '%s'.", var_name[node->return_type], top_function->vars[node->id].name.c_str()));
				if(!CanCast(node->return_type, result->return_type))
					t.Throw(Format("Can't cast from %s to %s for var assignment '%s'.", var_name[result->return_type], var_name[node->return_type], top_function->vars[node->id].name.c_str()));
				if(result->return_type != node->return_type)
				{
					Node* cast = NodePool.Get();
					cast->op = Node::N_CAST;
					cast->return_type = node->return_type;
					cast->nodes.push_back(result);
					node->nodes.push_back(cast);
				}
				else
					node->nodes.push_back(result);
				return node;
			}
			else
			{
				// equals
				t.Next();
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				node->id = JE;
				node->nodes.push_back(node);
			}
			break;
		case '+':
			{
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				onode->id = ADD;
				node->nodes.push_back(onode);
				t.Next();
			}
			break;
		case '-':
			{
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				onode->id = SUB;
				node->nodes.push_back(onode);
				t.Next();
			}
			break;
		case '*':
			{
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				onode->id = MUL;
				node->nodes.push_back(onode);
				t.Next();
			}
			break;
		case '/':
			{
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				onode->id = DIV;
				node->nodes.push_back(onode);
				t.Next();
			}
			break;
		case '%':
			{
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				onode->id = MOD;
				node->nodes.push_back(onode);
				t.Next();
			}
			break;
		case '!':
			t.Next();
			if(t.IsSymbol('='))
			{
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				onode->id = JNE;
				node->nodes.push_back(onode);
				t.Next();
			}
			else
				t.Unexpected();
		case '>':
			{
				t.Next();
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				if(t.IsSymbol('='))
				{
					onode->id = JGE;
					t.Next();
				}
				else
					onode->id = JG;
				node->nodes.push_back(onode);
			}
			break;
		case '<':
			{
				t.Next();
				Node* onode = NodePool.Get();
				onode->op = Node::N_OP;
				if(t.IsSymbol('='))
				{
					onode->id = JLE;
					t.Next();
				}
				else
					onode->id = JL;
				node->nodes.push_back(onode);
			}
			break;
		default:
			t.Unexpected();
			break;
		}
	}

	// if 1 element then simply return it
	if(node->nodes.size() == 1)
	{
		Node* result = node->nodes[0];
		node->nodes.clear();
		NodePool.Free(node);
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
			bool return_bool;
			VAR result = CanOp(a->return_type, (Op)n->id, b->return_type, return_bool);
			if(result == V_VOID)
				t.Throw(Format("Invalid operation %s %c %s.", var_name[a->return_type], OpChar(n->id), var_name[b->return_type]));
			if(a->return_type != result)
			{
				Node* cast = NodePool.Get();
				cast->op = Node::N_CAST;
				cast->return_type = result;
				cast->nodes.push_back(a);
				n->nodes.push_back(cast);
				n->nodes.push_back(b);
			}
			else if(b->return_type != result)
			{
				n->nodes.push_back(a);
				Node* cast = NodePool.Get();
				cast->op = Node::N_CAST;
				cast->return_type = result;
				cast->nodes.push_back(b);
				n->nodes.push_back(cast);
			}
			else
			{
				n->nodes.push_back(a);
				n->nodes.push_back(b);
			}
			n->return_type = return_bool ? V_BOOL : result;
			node->nodes.push_back(n);
		}
		else
			node->nodes.push_back(n);
	}

	// return result
	Node* result = node->nodes[0];
	node->nodes.clear();
	NodePool.Free(node);
	return result;
}

//=================================================================================================
static Node* parse_exprl()
{
	// exprl -> expr ;
	//
	Node* result = parse_expr(';');
	t.AssertSymbol(';');
	t.Next();
	return result;
}

//=================================================================================================
static void parse_vard(vector<Node*>& cont)
{
	// vard -> var_type name [= expr] [, name [= expr]] ... ;
	//		   var_type name ( [args] ) ;
	VAR type = (VAR)t.GetKeywordId();
	t.Next();
	bool first = true;

	do
	{
		const string& s = t.MustGetItem();

		// check is unique
		for(vector<ParseFunction*>::iterator it = functions.begin(), end = functions.end(); it != end; ++it)
		{
			if((*it)->name == s)
				t.Throw(Format("Can't use '%s' as variable name, function with same name exists.", s.c_str()));
		}
		for(vector<ParseVar>::iterator it = top_function->vars.begin(), end = top_function->vars.end(); it != end; ++it)
		{
			if(it->name == s)
				t.Throw(Format("Variable '%s' already declared at line %d with type '%s'.", s.c_str(), it->line, var_name[it->type]));
		}
		for(vector<ParseVar>::iterator it = top_function->vars.begin(), end = top_function->vars.end(); it != end; ++it)
		{
			if(it->name == s)
				t.Throw(Format("Can't use '%s' as variable name, argument with same name exists.", s.c_str()));
		}

		int vi = top_function->vars.size();
		ParseVar& v = Add1(top_function->vars);
		v.name = s;
		v.type = type;
		v.line = t.GetLine();

		t.Next();

		if(first && t.IsSymbol('('))
		{
			// it's a function
			t.Next();
			ParseFunction* f = FunctionPool.Get();
			f->name = top_function->vars.back().name;
			f->return_type = top_function->vars.back().type;
			top_function->vars.pop_back();
			functions.push_back(f);
			
			// args
			while(true)
			{
				if(t.IsKeywordGroup(2))
				{
					// type
					VAR var_type = (VAR)t.GetKeywordId();
					if(var_type == V_VOID)
						throw Format("Can't use void type as argument for funcion '%s'.", f->name.c_str());
					t.Next();
					// name
					const string& s = t.MustGetItem();
					for(vector<ParseFunction*>::iterator it = functions.begin(), end = functions.end(); it != end; ++it)
					{
						if((*it)->name == s)
							t.Throw(Format("Can't use '%s' as argument name, function with same name exists.", s.c_str()));
					}
					for(vector<ParseVar>::iterator it = top_function->vars.begin(), end = top_function->vars.end(); it != end; ++it)
					{
						if(it->name == s)
							t.Throw(Format("Argument name '%s' is already used with type %s.", s.c_str(), var_name[it->type]));
					}
					ParseVar& arg = Add1(f->args);
					arg.name = s;
					arg.type = var_type;
					arg.line = 0;
					f->arg_types.push_back(arg.type);
					t.Next();
					// , or )
					if(t.IsSymbol(','))
						t.Next();
				}
				else if(t.IsSymbol(')'))
				{
					t.Next();
					break;
				}
				else
					t.Unexpected(2, Tokenizer::T_SPECIFIC_KEYWORD_GROUP, 2, Tokenizer::T_SPECIFIC_SYMBOL, ')');
			}

			// {
			t.AssertSymbol('{');

			// function code block
			ParseFunction* prev_f = top_function;
			top_function = f;
			parse_block(f->nodes, '}');
			
			top_function = prev_f;			

			// }
			t.AssertSymbol('}');
			t.Next();
			return;
		}
		else
		{
			// it's a variable
			if(v.type == V_VOID)
				throw Format("Can't use void type variable '%s'.", v.name.c_str());
			if(t.IsSymbol('='))
			{
				// variable assignment
				t.Next();
				Node* result = parse_expr(';', ',');
				if(!result)
					t.Throw(Format("Empty assign expression for var %s '%s'.", var_name[v.type], v.name.c_str()));
				if(!CanCast(type, result->return_type))
					t.Throw(Format("Can't assign to var %s '%s' value of type '%s'.", var_name[v.type], v.name.c_str(), var_name[result->return_type]));
				Node* node = NodePool.Get();
				node->op = Node::N_SET;
				node->id = vi;
				node->return_type = v.type;
				if(type != result->return_type)
				{
					Node* cast = NodePool.Get();
					cast->op = Node::N_CAST;
					cast->return_type = type;
					cast->nodes.push_back(result);
					node->nodes.push_back(cast);
				}
				else
					node->nodes.push_back(result);
				cont.push_back(node);
			}
		}

		first = false;

		// next variable
		if(t.IsSymbol(','))
			t.Next();
		else
			break;
	}
	while(1);

	t.AssertSymbol(';');
	t.Next();
}

//=================================================================================================
static Node* parse_return()
{
	t.Next();
	Node* result = parse_expr(';');
	t.AssertSymbol(';');
	t.Next();
	bool invalid_type = false, need_cast = false;
	if(result)
	{
		if(!CanCast(top_function->return_type, result->return_type))
			invalid_type = true;
		else if(top_function->return_type != result->return_type)
			need_cast = true;
	}
	else
	{
		if(top_function->return_type != V_VOID)
			invalid_type = true;
	}
	if(invalid_type)
		throw Format("Function '%s' must return value of type %s.", top_function->name.c_str(), var_name[top_function->return_type]);
	Node* node = NodePool.Get();
	node->op = Node::N_RETURN;
	if(need_cast)
	{
		Node* cast = NodePool.Get();
		cast->op = Node::N_CAST;
		cast->return_type = top_function->return_type;
		cast->nodes.push_back(result);
		node->nodes.push_back(cast);
	}
	else
		node->nodes.push_back(result);
	node->return_type = top_function->return_type;
	return node;
}

//=================================================================================================
static Node* parse_if()
{
	// if
	t.Next();
	// (
	t.AssertSymbol('(');
	t.Next();
	// expr
	Node* result = parse_expr(')');
	if(!result)
		t.Throw("Empty if expression.");
	if(result->return_type != V_BOOL)
		t.Throw("If expression must return bool.");
	t.AssertSymbol(')');
	t.Next();
	// create node
	Node* node = NodePool.Get();
	node->op = Node::N_IF;
	node->return_type = V_VOID;
	node->nodes.push_back(result);
	// {
	t.AssertSymbol('{');
	t.Next();
	// block
	Node* block = NodePool.Get();
	block->op = Node::N_BLOCK;
	block->return_type = V_VOID;
	parse_block(block->nodes, '}');
	node->nodes.push_back(block);
	// }
	t.AssertSymbol('}');
	t.Next();
	// else
	if(t.IsKeyword(K_ELSE))
	{
		t.Next();
		// {
		Node* else_block = NodePool.Get();
		parse_block(else_block->nodes, '}');
		node->nodes.push_back(else_block);
		t.AssertSymbol('}');
		t.Next();
	}
	return node;
}

//=================================================================================================
static void parse_block(vector<Node*>& cont, char funcend)
{
	while(true)
	{
		if(funcend)
		{
			if(t.IsSymbol(funcend))
				break;
		}
		else
		{
			if(t.IsEof())
				break;
		}

		Node* node = NULL;
		if(t.IsKeywordGroup(0))
		{
			Keyword k = (Keyword)t.GetKeywordId();
			if(k == K_RETURN)
				node = parse_return();
			else
				node = parse_if();
		}
		else if(t.IsKeywordGroup(2))
			parse_vard(cont);
		else
			node = parse_exprl();

		if(node)
			cont.push_back(node);
	}
}

//=================================================================================================
static void parse_node(Node* node, bool top)
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
		code->push_back(CALL);
		code->push_back(node->id);
		break;
	case Node::N_CALLF:
		code->push_back(CALLF);
		code->push_back(node->id);
		break;
	case Node::N_CSTR:
		code->push_back(PUSH_CSTR);
		code->push_back(node->id);
		break;
	case Node::N_OP:
		code->push_back(node->id);
		break;
	case Node::N_VAR:
		code->push_back(PUSH_VAR);
		code->push_back(node->id);
		break;
	case Node::N_ARG:
		code->push_back(PUSH_ARG);
		code->push_back(node->id);
		break;
	case Node::N_SET:
		code->push_back(SET_VAR);
		code->push_back(node->id);
		break;
	case Node::N_INT:
		code->push_back(PUSH_INT);
		code->resize(code->size()+4);
		*((int*)(&*(code->end()-4))) = node->id;
		break;
	case Node::N_FLOAT:
		code->push_back(PUSH_FLOAT);
		code->resize(code->size()+4);
		*((float*)(&*(code->end()-4))) = node->fl;
		break;
	case Node::N_CAST:
		code->push_back(CAST);
		code->push_back(node->return_type);
		break;
	case Node::N_RETURN:
		code->push_back(RET);
		break;
	default:
		assert(0);
		break;
	}
	if(top && node->return_type != V_VOID)
	{
		// pop unused return value
		code->push_back(POP);
	}
}

//=================================================================================================
static void free_node(Node* node)
{
	for(vector<Node*>::iterator it = node->nodes.begin(), end = node->nodes.end(); it != end; ++it)
		free_node(*it);
	node->nodes.clear();
	NodePool.Free(node);
}

//=================================================================================================
bool parse(cstring file, ParseOutput& out)
{
	strs = &out.strs;
	code = &out.code;
	
	// setup tokenizer
	t.AddKeyword("return", K_RETURN);
	t.AddKeyword("if", K_IF);
	t.AddKeyword("else", K_ELSE);
	for(int i=0; i<n_funcs; ++i)
		t.AddKeyword(funcs[i].name, i, 1);
	t.AddKeyword("void", V_VOID, 2);
	t.AddKeyword("int", V_INT, 2);
	t.AddKeyword("float", V_FLOAT, 2);
	t.AddKeyword("string", V_STRING, 2);
	if(!t.FromFile(file))
	{
		printf("CLUED PARSE ERROR: Can't open file '%s'.", file);
		return false;
	}

	// init main function
	ParseFunction* f = FunctionPool.Get();
	f->return_type = V_VOID;
	f->name = "";
	functions.push_back(f);
	top_function = f;

	// parse...
	try
	{
		t.Next();
		parse_block(f->nodes, 0);
	}
	catch(cstring err)
	{
		printf("CLUED PARSE ERROR: %s", err);
		pause();
		return false;
	}

	// create byte code from expression tree and cleanup
	for(vector<ParseFunction*>::iterator it = functions.begin(), end = functions.end(); it != end; ++it)
	{
		ParseFunction& f = **it;
		top_function = &f;
		if(it != functions.begin())
		{
			ScriptFunction& sf = Add1(out.funcs);
			sf.have_result = (f.return_type != V_VOID);
			sf.args = f.args.size();
			sf.pos = code->size();
		}
		if(!f.vars.empty())
		{
			code->push_back(SET_VARS);
			code->push_back(f.vars.size());
		}
		for(vector<Node*>::iterator it = f.nodes.begin(), end = f.nodes.end(); it != end; ++it)
		{
			parse_node(*it, true);
			free_node(*it);
		}
		if(code->back() != RET)
			code->push_back(RET);
	}

	return true;
}
