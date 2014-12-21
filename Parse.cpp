#include "Pch.h"
#include "Base.h"
#include "Parse.h"
#include "Var.h"
#include "Function.h"
#include "Op.h"

//=================================================================================================
VAR CanOp(VAR a, Op op, VAR b)
{
	switch(op)
	{
	case ADD:
		if(a == b)
		{
			if(a != V_VOID)
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
		N_OP,
		N_VAR,
		N_SET,
		N_INT,
		N_FLOAT,
		N_CSTR,
		N_CAST
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
static ObjectPool<Node> NodePool;
static vector<ParseVar> vars;
static vector<Node*> top_nodes;
static vector<Node*> node_out, node_stack;

//=================================================================================================
static Node* parse_expr(char funcend, char funcend2=0);

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
				if(f.args[i] != result->return_type)
				{
					Node* cast = NodePool.Get();
					cast->op = Node::N_CAST;
					cast->return_type = f.args[i];
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
		return fnode;
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
		Node* vnode = NodePool.Get();
		vnode->op = Node::N_VAR;
		vnode->id = index;
		vnode->return_type = v.type;
		t.Next();
		return vnode;
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

		if(t.IsSymbol(funcend) || t.IsSymbol(funcend2))
			break;
		else if(t.IsSymbol('='))
		{
			if(node->nodes.size() > 1 || node->nodes[0]->op != Node::N_VAR)
				t.Throw("Can't assign becouse lvalue is not var.");
			node->op = Node::N_SET;
			node->id = node->nodes[0]->id;
			node->return_type = node->nodes[0]->return_type;
			node->nodes[0]->nodes.clear();
			NodePool.Free(node->nodes[0]);
			node->nodes.clear();
			t.Next();
			Node* result = parse_expr(funcend);
			if(!result)
				t.Throw(Format("Can't assign empty expression to var %s '%s'.", var_name[node->return_type], vars[node->id].name.c_str()));
			if(!CanCast(node->return_type, result->return_type))
				t.Throw(Format("Can't cast from %s to %s for var assignment '%s'.", var_name[result->return_type], var_name[node->return_type], vars[node->id].name.c_str()));
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
		else if(t.IsSymbol('+') || t.IsSymbol('-') || t.IsSymbol('*') || t.IsSymbol('/'))
		{
			Node* onode = NodePool.Get();
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
			VAR result = CanOp(a->return_type, (Op)n->id, b->return_type);
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
			n->return_type = result;
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
static void parse_exprl()
{
	// exprl -> expr ;
	Node* result = parse_expr(';');
	t.AssertSymbol(';');
	top_nodes.push_back(result);
}

//=================================================================================================
static void parse_vard()
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
		for(vector<ParseVar>::iterator it = vars.begin(), end = vars.end(); it != end; ++it)
		{
			if(it->name == s)
				t.Throw(Format("Variable '%s' already declared at line %d with type '%s'.", s.c_str(), it->line, var_name[it->type]));
		}
		int vi = vars.size();
		ParseVar& v = Add1(vars);
		v.name = s;
		v.type = type;
		v.line = t.GetLine();

		t.Next();

		// assingment
		if(t.IsSymbol('='))
		{
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
			top_nodes.push_back(node);
		}
		else if(first && t.IsSymbol('('))
		{
			// it's a function
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
	for(int i=0; i<n_funcs; ++i)
		t.AddKeyword(funcs[i].name, i, 1);
	t.AddKeyword("int", V_INT, 2);
	t.AddKeyword("float", V_FLOAT, 2);
	t.AddKeyword("string", V_STRING, 2);
	if(!t.FromFile(file))
	{
		printf("CLUED PARSE ERROR: Can't open file '%s'.", file);
		return false;
	}

	// parse...
	try
	{
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
	}
	catch(cstring err)
	{
		printf("CLUED PARSE ERROR: %s", err);
		pause();
		return false;
	}

	// create byte code from expression tree and cleanup
	if(vars.size() > 0)
	{
		code->push_back(SET_VARS);
		code->push_back(vars.size());
	}
	for(vector<Node*>::iterator it = top_nodes.begin(), end = top_nodes.end(); it != end; ++it)
	{
		parse_node(*it, true);
		free_node(*it);
	}
	code->push_back(RET);

	return true;
}
