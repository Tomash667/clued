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
struct ParseBlock;
struct ParseFunction;

//=================================================================================================
struct ParseVar
{
	string name;
	VAR type;
	int line, index, local_index;
	ParseBlock* block;
};

//=================================================================================================
struct Node
{
	enum NodeOp
	{
		N_CALL,
		N_CALLF,
		N_OP,
		N_OPS,
		N_VAR,
		N_ARG,
		N_SET,
		N_INT,
		N_FLOAT,
		N_CSTR,
		N_CAST,
		N_RETURN,
		N_IF,
		N_BLOCK,
		N_POP,
	} op;
	union
	{
		int id;
		float fl;
	};
	vector<Node*> nodes;
	VAR return_type;

	inline void push(Node* node)
	{
		nodes.push_back(node);
	}

	inline void clear()
	{
		nodes.clear();
	}
};

//=================================================================================================
struct ParseBlock
{
	vector<ParseBlock*> blocks;
	vector<ParseVar> vars;
	ParseFunction* function;
	ParseBlock* parent;
	uint max_vars, used_vars;
};

//=================================================================================================
struct ParseFunction
{
	string name;
	VAR return_type;
	vector<Node*> nodes;
	vector<ParseVar> args;
	vector<VAR> arg_types;
	ParseBlock* block;
	int index;
};

//=================================================================================================
static ObjectPool<Node> NodePool;
static ObjectPool<ParseBlock> BlockPool;
static ObjectPool<ParseFunction> FunctionPool;
static vector<Node*> node_out, node_stack;
static vector<ParseFunction*> functions;

//=================================================================================================
inline Node* create_node(Node::NodeOp op)
{
	Node* node = NodePool.Get();
	node->op = op;
	return node;
}

//=================================================================================================
inline void free_node(Node* node)
{
	node->clear();
	NodePool.Free(node);
}

//=================================================================================================
ParseVar* find_var(ParseBlock* block, int index)
{
	for(ParseVar& v : block->vars)
	{
		if (v.index == index)
			return &v;
	}

	if (block->parent)
		return find_var(block->parent, index);
	else
		return NULL;
}

//=================================================================================================
ParseVar* find_var(ParseBlock* block, const string& name)
{
	for(ParseVar& v : block->vars)
	{
		if (v.name == name)
			return &v;
	}

	if (block->parent)
		return find_var(block->parent, name);

	return NULL;
}

//=================================================================================================
enum NamedType
{
	NT_NONE,
	NT_VAR,
	NT_ARG,
	NT_FUNC
};

//=================================================================================================
NamedType find_named(ParseBlock* block, const string& name, void*& result)
{
	for each(ParseFunction* f in functions)
	{
		if (f->name == name)
		{
			result = f;
			return NT_FUNC;
		}
	}

	for(ParseVar& v : block->function->args)
	{
		if (v.name == name)
		{
			result = &v;
			return NT_ARG;
		}
	}

	ParseVar* v = find_var(block, name);
	if (v)
	{
		result = v;
		return NT_VAR;
	}

	return NT_NONE;
}

//=================================================================================================
int add_var_index(ParseBlock* block)
{
	if (block->used_vars < block->max_vars)
	{
		block->used_vars++;
		return block->used_vars - 1;
	}
	else
	{
		block->max_vars++;
		block->used_vars++;
		while (block->parent)
		{
			block = block->parent;
			block->max_vars++;
		}
		return block->max_vars - 1;
	}
}

//=================================================================================================
ParseVar& add_var(ParseBlock* block, const string& name, VAR type)
{
	ParseVar& v = Add1(block->vars);
	v.name = name;
	v.type = type;
	v.local_index = block->vars.size() - 1;
	v.index = add_var_index(block);
	return v;
}

//=================================================================================================
static void parse_args(ParseBlock* block, Node* fnode, const string& name, const vector<VAR>& args);
static Node* parse_basic_statement(ParseBlock* block);
static Node* parse_block(ParseBlock* block, char endc);
static Node* parse_if(ParseBlock* block);
static Node* parse_line_block(ParseBlock* block);
static Node* parse_line(ParseBlock* block);
static bool parse_op(ParseBlock* block, char endc, Node* node, Op oper, cstring oper_name);
static Node* parse_return(ParseBlock* block);
static Node* parse_statement(ParseBlock* block, char endc, char endc2 = 0);
static Node* parse_vard(ParseBlock* block);

//=================================================================================================
static void parse_args(ParseBlock* block, Node* fnode, const string& name, const vector<VAR>& args)
{
	// (
	t.Next();
	t.AssertSymbol('(');
	t.Next();
	// args
	if (!args.empty())
	{
		int index = 1;
		for (vector<VAR>::const_iterator it = args.begin(), end = args.end(); it != end; ++it, ++index)
		{
			VAR type = *it;
			Node* result = parse_statement(block, it + 1 != end ? ',' : ')');
			if (!result)
				t.Throw(Format("Empty statement as %d arg of function %s.", index, name.c_str()));
			if (!CanCast(type, result->return_type))
				t.Throw(Format("Can't cast from %s to %s for arg %d in function call %s.", var_name[result->return_type], var_name[type], index, name.c_str()));
			if (type != result->return_type)
			{
				Node* cast = create_node(Node::N_CAST);
				cast->return_type = type;
				cast->push(result);
				fnode->push(cast);
			}
			else
				fnode->push(result);
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
static Node* parse_basic_statement(ParseBlock* block)
{
	if (t.IsKeywordGroup(1))
	{
		// func
		int fid = t.GetKeywordId();
		const Function& f = funcs[fid];
		Node* fnode = create_node(Node::N_CALL);
		fnode->id = fid;
		fnode->return_type = f.return_type;
		parse_args(block, fnode, f.name, f.args);
		return fnode;
	}
	else if (t.IsItem())
	{
		// var, arg or script function
		void* result;
		NamedType type = find_named(block, t.GetItem(), result);
		switch (type)
		{
		case NT_VAR:
			{
				ParseVar& v = *(ParseVar*)result;
				Node* vnode = create_node(Node::N_VAR);
				vnode->id = v.index;
				vnode->return_type = v.type;
				t.Next();
				return vnode;
			}
		case NT_ARG:
			{
				ParseVar& v = *(ParseVar*)result;
				Node* vnode = create_node(Node::N_ARG);
				vnode->id = v.index;
				vnode->return_type = v.type;
				t.Next();
				return vnode;
			}
		case NT_FUNC:
			{
				ParseFunction& f = *(ParseFunction*)result;
				Node* fnode = create_node(Node::N_CALLF);
				fnode->id = f.index;
				fnode->return_type = f.return_type;
				parse_args(block, fnode, f.name, f.arg_types);
				return fnode;
			}
		}
	}
	else if (t.IsString())
	{
		// string literal
		int si = strs->size();
		Str* s = StrPool.Get();
		s->refs = 1;
		Unescape(t.GetString(), s->s);
		strs->push_back(s);
		t.Next();
		Node* csnode = create_node(Node::N_CSTR);
		csnode->id = si;
		csnode->return_type = V_STRING;
		return csnode;
	}
	else if (t.IsInt())
	{
		// int literal
		Node* inode = create_node(Node::N_INT);
		inode->id = t.GetInt();
		inode->return_type = V_INT;
		t.Next();
		return inode;
	}
	else if (t.IsFloat())
	{
		// float literal
		Node* fnode = create_node(Node::N_FLOAT);
		fnode->fl = t.GetFloat();
		fnode->return_type = V_FLOAT;
		t.Next();
		return fnode;
	}
	else if (t.IsSymbol('-'))
	{
		// -a
		t.Next();
		Node* result = parse_basic_statement(block);
		if (result->return_type == V_INT || result->return_type == V_FLOAT)
		{
			Node* node = create_node(Node::N_OP);
			node->id = NEG;
			node->return_type = V_INT;
			node->push(result);
			return node;
		}
		else
			t.Throw(Format("Can't use unrary minus on %s.", var_name[result->return_type]));
	}
	else if (t.IsSymbol('+'))
	{
		// +a
		Node* result = parse_basic_statement(block);
		if (result->return_type == V_INT || result->return_type == V_FLOAT)
			return result;
		else
			t.Throw(Format("Can't use unrary plus on %s.", var_name[result->return_type]));
	}
	else if (t.IsSymbol('('))
	{
		// (
		t.Next();
		Node* result = parse_statement(block, ')');
		t.AssertSymbol(')');
		t.Next();
		return result;
	}

	t.Unexpected();
	assert(0);
	return NULL;
}

//=================================================================================================
static Node* parse_block(ParseBlock* block, char endc)
{
	Node* node = create_node(Node::N_BLOCK);

	while(true)
	{
		if(endc)
		{
			if(t.IsSymbol(endc))
				break;
		}
		else
		{
			if(t.IsEof())
				break;
		}

		Node* result;
		if(t.IsKeywordGroup(2))
			result = parse_vard(block);
		else
			result = parse_line(block);
		if(result)
		{
			node->push(result);
			if(result->return_type != V_VOID)
				node->push(create_node(Node::N_POP));
		}
	}

	if(node->nodes.empty())
	{
		NodePool.Free(node);
		return NULL;
	}
	else
		return node;
}

//=================================================================================================
static Node* parse_if(ParseBlock* block)
{
	Node* node = create_node(Node::N_IF);
	node->return_type = V_VOID;
	// if
	t.Next();
	// (
	t.AssertSymbol('(');
	t.Next();
	// expr
	Node* result = parse_statement(block, ')');
	if(!result)
		t.Throw("Empty if expression.");
	if(result->return_type != V_BOOL)
		t.Throw("If expression must return bool.");
	node->push(result);
	t.AssertSymbol(')');
	t.Next();
	// block, possibly added NULL Node
	node->push(parse_line_block(block));
	// else
	if(t.IsKeyword(K_ELSE))
	{
		t.Next();
		Node* else_block = parse_line_block(block);
		if(else_block)
			node->push(else_block);
	}
	return node;
}

//=================================================================================================
static Node* parse_line_block(ParseBlock* block)
{
	if(t.IsSymbol('{'))
	{
		t.Next();
		ParseBlock* block2 = BlockPool.Get();
		block2->function = block->function;
		block2->parent = block;
		block2->max_vars = block->max_vars;
		block2->used_vars = block->used_vars;
		block->blocks.push_back(block2);
		Node* result = parse_block(block2, '}');
		t.AssertSymbol('}');
		t.Next();
		return result;
	}
	else
		return parse_line(block);
}

//=================================================================================================
static Node* parse_line(ParseBlock* block)
{
	if (t.IsSymbol(';'))
	{
		t.Next();
		return NULL;
	}
	else if(t.IsKeywordGroup(0))
	{
		Keyword k = (Keyword)t.GetKeywordId();
		if(k == K_IF)
			return parse_if(block);
		else if(k == K_RETURN)
			return parse_return(block);
		else
			t.Unexpected();
	}
	else
	{
		Node* node = parse_statement(block, ';');
		t.AssertSymbol(';');
		t.Next();
		return node;
	}
	return NULL;
}

//=================================================================================================
static bool parse_op(ParseBlock* block, char endc, Node* node, Op oper, cstring oper_name)
{
	t.Next();
	if (t.IsSymbol('='))
	{
		// compound operation
		t.Next();
		if (node->nodes.size() > 1 || node->nodes[0]->op != Node::N_VAR)
			t.Throw(Format("Can't compund %s becouse lvalue is not var.", oper_name));
		node->op = Node::N_SET;
		node->id = node->nodes[0]->id;
		node->return_type = node->nodes[0]->return_type;
		free_node(node->nodes[0]);
		node->nodes.clear();

		// parse rvalue
		Node* result = parse_statement(block, endc);
		ParseVar& v = *find_var(block, node->id);
		if (!result)
			t.Throw(Format("Can't compound %s empty expression to var %s '%s'.", oper_name, var_name[node->return_type], v.name.c_str()));
		bool return_bool = false;
		VAR return_type = CanOp(v.type, oper, result->return_type, return_bool);
		if (return_type == V_VOID || return_bool)
			t.Throw(Format("Can't compound %s variable %s '%s' and %s.", oper_name, var_name[node->return_type], v.name.c_str(), var_name[return_type]));

		// create expression tree
		Node* op = create_node(Node::N_OP);
		op->id = oper;
		op->return_type = return_type;
		Node* get_var = create_node(Node::N_VAR);
		get_var->id = v.index;
		get_var->return_type = v.type;
		if (v.type != return_type)
		{
			Node* cast = create_node(Node::N_CAST);
			cast->id = return_type;
			cast->return_type = return_type;
			cast->push(get_var);
			op->push(cast);
		}
		else
			op->push(get_var);
		if (result->return_type != return_type)
		{
			Node* cast = create_node(Node::N_CAST);
			cast->id = return_type;
			cast->return_type = return_type;
			cast->push(result);
			op->push(cast);
		}
		else
			op->push(result);
		if (v.type != return_type)
		{
			Node* cast = create_node(Node::N_CAST);
			cast->id = v.type;
			cast->return_type = v.type;
			cast->push(op);
			node->push(cast);
		}
		else
			node->push(op);
		return true;
	}
	else
	{
		// normal operation
		Node* onode = create_node(Node::N_OPS);
		onode->id = oper;
		node->push(onode);
		return false;
	}
}

//=================================================================================================
static Node* parse_return(ParseBlock* block)
{
	t.Next();
	Node* result = parse_statement(block, ';');
	t.AssertSymbol(';');
	t.Next();
	bool invalid_type = false, need_cast = false;
	if (result)
	{
		if (!CanCast(block->function->return_type, result->return_type))
			invalid_type = true;
		else if (block->function->return_type != result->return_type)
			need_cast = true;
	}
	else
	{
		if (block->function->return_type != V_VOID)
			invalid_type = true;
	}
	if (invalid_type)
	{
		if (block->function->index != -1)
			throw Format("Function '%s' must return value of type %s.", block->function->name.c_str(), var_name[block->function->return_type]);
		else
			throw "Main function must return void.";
	}
	Node* node = create_node(Node::N_RETURN);
	if (need_cast)
	{
		Node* cast = create_node(Node::N_CAST);
		cast->return_type = block->function->return_type;
		cast->push(result);
		node->push(cast);
	}
	else
		node->push(result);
	node->return_type = block->function->return_type;
	return node;
}

//=================================================================================================
static Node* parse_statement(ParseBlock* block, char endc, char endc2)
{
	// expr -> expr [op expr]
	if (t.IsSymbol(endc) || t.IsSymbol(endc2))
		return NULL;

	Node* node = NodePool.Get();

	while (true)
	{
		Node* result = parse_basic_statement(block);
		node->push(result);

		if (!t.IsSymbol())
			t.Unexpected();

		if (t.IsSymbol(endc) || t.IsSymbol(endc2))
			break;

		switch (t.GetSymbol())
		{
		case '=':
			t.Next();
			if (!t.IsSymbol('='))
			{
				// assingment
				if (node->nodes.size() > 1 || node->nodes[0]->op != Node::N_VAR)
					t.Throw("Can't assign becouse lvalue is not var.");
				node->op = Node::N_SET;
				node->id = node->nodes[0]->id;
				node->return_type = node->nodes[0]->return_type;
				free_node(node->nodes[0]);
				node->nodes.clear();
				Node* result = parse_statement(block, endc);
				ParseVar& v = *find_var(block, node->id);
				if (!result)
					t.Throw(Format("Can't assign empty expression to var %s '%s'.", var_name[node->return_type], v.name.c_str()));
				if (!CanCast(node->return_type, result->return_type))
					t.Throw(Format("Can't cast from %s to %s for var assignment '%s'.", var_name[result->return_type], var_name[node->return_type], v.name.c_str()));
				if (result->return_type != node->return_type)
				{
					Node* cast = create_node(Node::N_CAST);
					cast->return_type = node->return_type;
					cast->push(result);
					node->push(cast);
				}
				else
					node->push(result);
				return node;
			}
			else
			{
				// equals
				t.Next();
				Node* onode = create_node(Node::N_OPS);
				onode->id = JE;
				node->push(onode);
			}
			break;
		case '+':
			if (parse_op(block, endc, node, ADD, "add"))
				return node;
			break;
		case '-':
			if (parse_op(block, endc, node, SUB, "subtract"))
				return node;
			break;
		case '*':
			if (parse_op(block, endc, node, MUL, "multiply"))
				return node;
			break;
		case '/':
			if (parse_op(block, endc, node, DIV, "divide"))
				return node;
			break;
		case '%':
			if (parse_op(block, endc, node, MOD, "modulo"))
				return node;
			break;
		case '!':
			t.Next();
			if (t.IsSymbol('='))
			{
				Node* onode = create_node(Node::N_OPS);
				onode->id = JNE;
				node->push(onode);
				t.Next();
			}
			else
				t.Unexpected();
		case '>':
			{
				t.Next();
				Node* onode = create_node(Node::N_OPS);
				if (t.IsSymbol('='))
				{
					onode->id = JGE;
					t.Next();
				}
				else
					onode->id = JG;
				node->push(onode);
			}
			break;
		case '<':
			{
				t.Next();
				Node* onode = create_node(Node::N_OPS);
				if (t.IsSymbol('='))
				{
					onode->id = JLE;
					t.Next();
				}
				else
					onode->id = JL;
				node->push(onode);
			}
			break;
		default:
			t.Unexpected();
			break;
		}
	}

	// if 1 element then simply return it
	if (node->nodes.size() == 1)
	{
		Node* result = node->nodes[0];
		free_node(node);
		return result;
	}

	// convert infix to RPN
	node_out.clear();
	node_stack.clear();
	for (vector<Node*>::iterator it = node->nodes.begin(), end = node->nodes.end(); it != end; ++it)
	{
		Node* n = *it;
		if (n->op == Node::N_OPS)
		{
			if (!node_stack.empty())
			{
				int prio = GetPriority(n->id);
				while (!node_stack.empty() && prio <= GetPriority(node_stack.back()->id))
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
	while (!node_stack.empty())
	{
		node_out.push_back(node_stack.back());
		node_stack.pop_back();
	}

	// conver RPN to tree
	node->nodes.clear();
	for (vector<Node*>::iterator it = node_out.begin(), end = node_out.end(); it != end; ++it)
	{
		Node* n = *it;
		if (n->op == Node::N_OPS)
		{
			Node* b = node->nodes.back();
			node->nodes.pop_back();
			Node* a = node->nodes.back();
			node->nodes.pop_back();
			bool return_bool = false;
			VAR result = CanOp(a->return_type, (Op)n->id, b->return_type, return_bool);
			if (result == V_VOID)
				t.Throw(Format("Invalid operation %s %s %s.", var_name[a->return_type], OpChar(n->id), var_name[b->return_type]));
			if (a->return_type != result)
			{
				Node* cast = create_node(Node::N_CAST);
				cast->return_type = result;
				cast->push(a);
				n->push(cast);
				n->push(b);
			}
			else if (b->return_type != result)
			{
				n->push(a);
				Node* cast = create_node(Node::N_CAST);
				cast->return_type = result;
				cast->push(b);
				n->push(cast);
			}
			else
			{
				n->push(a);
				n->push(b);
			}
			n->return_type = (return_bool ? V_BOOL : result);
			n->op = Node::N_OP;
			node->push(n);
		}
		else
			node->push(n);
	}

	// return result
	Node* result = node->nodes[0];
	free_node(node);
	return result;
}

//=================================================================================================
static Node* parse_vard(ParseBlock* block)
{
	// vard -> var_type name [= expr] [, name [= expr]] ... ;
	//		   var_type name ( [args] ) ;
	VAR type = (VAR)t.GetKeywordId();
	t.Next();
	bool first = true;
	Node* cont = create_node(Node::N_BLOCK);
	cont->return_type = V_VOID;

	do
	{
		// check is unique
		string s = t.MustGetItem();
		void* result;
		NamedType ntype = find_named(block, s, result);
		if (ntype != NT_NONE)
		{
			if (ntype == NT_VAR)
			{
				ParseVar& v = *(ParseVar*)result;
				t.Throw(Format("Variable '%s' already declared at line %d with type '%s'.", s.c_str(), v.line, var_name[v.type]));
			}
			else if (ntype == NT_ARG)
				t.Throw(Format("Can't use '%s' as variable name, argument with same name exists.", s.c_str()));
			else
				t.Throw(Format("Can't use '%s' as variable name, function with same name exists.", s.c_str()));
		}

		int line = t.GetLine();
		t.Next();

		if(first && t.IsSymbol('('))
		{
			// it's a function
			t.Next();
			ParseFunction* f = FunctionPool.Get();
			f->name = s;
			f->return_type = type;
			f->index = functions.size() - 1;
			f->block = BlockPool.Get();
			f->block->function = f;
			f->block->max_vars = 0;
			f->block->used_vars = 0;
			f->block->parent = NULL;
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
					void* result;
					NamedType ntype = find_named(f->block, s, result);
					if (ntype == NT_FUNC)
						t.Throw(Format("Can't use '%s' as argument name, function with same name exists.", s.c_str()));
					else if (ntype == NT_ARG)
					{
						ParseVar& v = *(ParseVar*)result;
						t.Throw(Format("Argument name '%s' is already used with type %s.", s.c_str(), var_name[v.type]));
					}
					ParseVar& arg = Add1(f->args);
					arg.name = s;
					arg.type = var_type;
					arg.line = 0;
					arg.index = arg.local_index = f->args.size() - 1;
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
			t.Next();

			// function code block
			Node* result = parse_block(f->block, '}');
			f->nodes = result->nodes;
			free_node(result);

			// }
			t.AssertSymbol('}');
			t.Next();
			NodePool.Free(cont);
			return NULL;
		}
		else
		{
			// it's a variable
			if(type == V_VOID)
				throw Format("Can't use void type variable '%s'.", s.c_str());
			ParseVar& v = add_var(block, s, type);
			v.line = line;
			v.block = block;
			if(t.IsSymbol('='))
			{
				// variable assignment
				t.Next();
				Node* result = parse_statement(block, ';', ',');
				if(!result)
					t.Throw(Format("Empty assign expression for var %s '%s'.", var_name[v.type], v.name.c_str()));
				if(!CanCast(type, result->return_type))
					t.Throw(Format("Can't assign to var %s '%s' value of type '%s'.", var_name[v.type], v.name.c_str(), var_name[result->return_type]));
				Node* node = create_node(Node::N_SET);
				node->id = v.index;
				node->return_type = v.type;
				if(type != result->return_type)
				{
					Node* cast = create_node(Node::N_CAST);
					cast->return_type = type;
					cast->push(result);
					node->push(cast);
				}
				else
					node->push(result);
				cont->push(node);
				cont->push(create_node(Node::N_POP));
			}
		}

		first = false;

		// next variable
		if(t.IsSymbol(','))
			t.Next();
		else
			break;
	} while(1);

	t.AssertSymbol(';');
	t.Next();
	return cont;
}

vector<uint> jumps;

//=================================================================================================
static int add_jump()
{
	uint pos = code->size();
	jumps.push_back(pos);
	return jumps.size() - 1;
}

//=================================================================================================
static void apply_jump(int id)
{
	uint pos = code->size();
	uint pos2 = jumps[id];
	short offset = (short)(pos - pos2);
	*(short*)(&(*code)[0] + pos2) = offset;
}

//=================================================================================================
static void parse_node(Node* node, bool inside_if=false)
{
	if (node->op != Node::N_IF)
	{
		for (vector<Node*>::iterator it = node->nodes.begin(), end = node->nodes.end(); it != end; ++it)
			parse_node(*it);
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
		if (node->id < CMP)
			code->push_back(node->id);
		else
		{
			code->push_back(CMP);
			if (inside_if)
				code->push_back(node->id);
			else
				code->push_back(node->id + 6);
		}
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
	case Node::N_BLOCK:
		break;
	case Node::N_POP:
		code->push_back(POP);
		break;
	case Node::N_IF:
	{
		// if expression
		parse_node(node->nodes[0], true);
		if (node->nodes[0]->op != Node::N_OPS)
		{
			code->push_back(TEST);
			code->push_back(JE);
		}
		int jid = add_jump();
		code->push_back(0);
		code->push_back(0);
		// jmp to else / end
		code->push_back(JMP);
		int jid2 = add_jump();
		code->push_back(0);
		code->push_back(0);
		// if block
		apply_jump(jid);
		if (node->nodes[1])
			parse_node(node->nodes[1]);
		if (node->nodes.size() > 2)
		{
			// jump to end
			code->push_back(JMP);
			int jid3 = add_jump();
			code->push_back(0);
			code->push_back(0);
			// else block
			apply_jump(jid2);
			parse_node(node->nodes[2]);
			// past else block
			apply_jump(jid3);
		}
		break;
	}
	default:
		assert(0);
		break;
	}
}

//=================================================================================================
static void clean_node(Node* node)
{
	for(vector<Node*>::iterator it = node->nodes.begin(), end = node->nodes.end(); it != end; ++it)
		clean_node(*it);
	node->nodes.clear();
	NodePool.Free(node);
}

//=================================================================================================
bool parse(cstring file, ParseOutput& out, bool halt)
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
	t.AddKeyword("bool", V_BOOL, 2);
	t.AddKeyword("int", V_INT, 2);
	t.AddKeyword("float", V_FLOAT, 2);
	t.AddKeyword("string", V_STRING, 2);
	if(!t.FromFile(file))
	{
		printf("CLUED PARSE ERROR: Can't open file '%s'.", file);
		if(halt)
			pause();
		return false;
	}

	// init main function & block
	ParseFunction* f = FunctionPool.Get();
	f->return_type = V_VOID;
	f->name = "";
	f->index = -1;
	f->block = BlockPool.Get();
	f->block->function = f;
	f->block->parent = NULL;
	f->block->max_vars = 0;
	f->block->used_vars = 0;
	functions.push_back(f);

	// parse...
	try
	{
		t.Next();
		Node* result = parse_block(f->block, 0);
		f->nodes = result->nodes;
		free_node(result);
	}
	catch(cstring err)
	{
		printf("CLUED PARSE ERROR: %s", err);
		if(halt)
			pause();
		for(vector<ParseFunction*>::iterator it = functions.begin(), end = functions.end(); it != end; ++it)
		{
			ParseFunction& f = **it;
			for(vector<Node*>::iterator it2 = f.nodes.begin(), end2 = f.nodes.end(); it2 != end2; ++it2)
				clean_node(*it2);
			FunctionPool.Free(*it);
		}
		functions.clear();
		return false;
	}

	// create byte code from expression tree and cleanup
	for(vector<ParseFunction*>::iterator it = functions.begin(), end = functions.end(); it != end; ++it)
	{
		ParseFunction& f = **it;
		if(it != functions.begin())
		{
			ScriptFunction& sf = Add1(out.funcs);
			sf.have_result = (f.return_type != V_VOID);
			sf.args = f.args.size();
			sf.pos = code->size();
		}
		if(f.block->max_vars)
		{
			code->push_back(SET_VARS);
			code->push_back(f.block->max_vars);
		}
		for(vector<Node*>::iterator it2 = f.nodes.begin(), end2 = f.nodes.end(); it2 != end2; ++it2)
		{
			parse_node(*it2);
			clean_node(*it2);
		}
		if(code->back() != RET)
			code->push_back(RET);
		FunctionPool.Free(*it);
	}
	functions.clear();

	return true;
}
