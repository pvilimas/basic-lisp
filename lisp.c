#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// lisp-like calculator (uses prefix notation)

#define panic(fmt, ...) \
	do { \
		fprintf(stderr, \
			"runtime error: " fmt "\n" __VA_OPT__(,) __VA_ARGS__); \
		exit(1); \
	} while(0)

// step 1: program string to list of tokens

typedef enum {
	T_NONE,
	T_OPEN_PAREN, // 1 char
	T_CLOSE_PAREN, // 1 char
	T_ATOM // <len> chars
} TokenType;

typedef struct {
	TokenType type;
	
	char* atom_str;
	int atom_len;
} Token;

#define token_fmt \
	"Token{%s, '%.*s'}"

#define token_type_str(ttype) \
	((ttype)==T_NONE \
		? "T_NONE" \
	: ((ttype)==T_OPEN_PAREN \
		? "T_OPEN_PAREN" \
	: ((ttype)==T_CLOSE_PAREN \
		? "T_CLOSE_PAREN" \
	: ((ttype)==(T_ATOM) \
		? "T_ATOM" \
	: "T_ERROR_UNKNOWN_TYPE"))))

#define token_arg(t) \
	(token_type_str((t).type)), (t).atom_len, (t).atom_str

typedef struct {
	Token* tokens;
	int len;
} TokenList;

#define tl_new() \
	((TokenList){0})

#define tl_resize(tl, n) \
	do { \
		(tl).len = (n); \
		(tl).tokens = realloc((tl).tokens, sizeof(Token) * (tl).len); \
	} while(0)

#define tl_append(tl, ... ) \
	do { \
		tl_resize((tl), (tl).len + 1); \
		(tl).tokens[(tl).len - 1] = (__VA_ARGS__); \
	} while(0)

#define tl_print(tl) \
	do { \
		for (int i = 0; i < (tl).len; i++) { \
			printf("\t[%d] " token_fmt "\n", i, token_arg((tl).tokens[i])); \
		} \
	} while(0)

TokenList tokenize(char* prog) {
	TokenList l = tl_new();
	
	int i = 0;
	int n = strlen(prog);
	while (i < n) {
		char c = prog[i];

		if (c == '(') {
			tl_append(l, (Token){.type=T_OPEN_PAREN});
		} else if (c == ')') {
			tl_append(l, (Token){.type=T_CLOSE_PAREN});
		} else if (c == '\n' || isspace(c)) {
			i++;
			continue;
		} else {
			Token t = { .type=T_ATOM, .atom_str=&prog[i], .atom_len=1 };
			while (prog[i] != '(' && prog[i] != ')' && !isspace(prog[i])) {
				t.atom_len++;
				i++;
			}
			// remove extra character
			t.atom_len -= 1;
			i--;

			// avoid appending Token{""}
			if (t.atom_len > 0) {
				tl_append(l, t);
			}
		}
		
		i++;
	}

	return l;
}

// step 2: list of tokens to ast tree

typedef enum {
	A_NONE,
	A_ATOM, // singular item
	A_LIST // a list of other ASTNodes
} ASTType;

typedef struct ASTNode {
	ASTType type;
	union {
		// A_ATOM
		struct { char* atom_str; int atom_len; };
		// A_LIST
		struct { struct ASTNode** list_items; int list_len; };
	};
} ASTNode;

#define node_new() \
	(calloc(1, sizeof(ASTNode)))

void node_print_rec(ASTNode* node, int level) {
	for (int i = 0; i < level; i++) {
		putc('\t', stdout);
		putc('|', stdout);
	}
	if (node->type == A_ATOM) {
		printf("ASTNode<type=A_ATOM, \"%.*s\">\n",
			node->atom_len,
			node->atom_str);
	} else if (node->type == A_LIST) {
		printf("ASTNode<type=A_LIST, %d items>:\n",
			node->list_len);
		for (int i = 0; i < node->list_len; i++) {
			node_print_rec(node->list_items[i], level + 1);
		}
	} else {
		printf("ASTNode<type=A_UNIMPLEMENTED: see line %d>\n", __LINE__);
	}
}

#define node_print(node) \
	(node_print_rec((node), 0))

#define list_resize(node, n) \
	do { \
		(node)->list_len = (n); \
		(node)->list_items = realloc((node)->list_items, sizeof(ASTNode*) * (node)->list_len); \
	} while(0)

// arg must be a ASTNode*
#define list_append(node, ...) \
	do { \
		list_resize((node), (node)->list_len + 1); \
		(node)->list_items[(node)->list_len - 1] = (__VA_ARGS__); \
	} while(0)

// takes only a single token
ASTNode* make_ast_single(Token t) {

	ASTNode* root = node_new();
	root->type = A_ATOM;
	root->atom_str = t.atom_str;
	root->atom_len = t.atom_len;
	return root;
}

// assumes OPEN_PAREN, ..., CLOSE_PAREN
ASTNode* make_ast_list_simple(TokenList tl) {

	ASTNode* root = node_new();
	root->type = A_LIST;

	// level of nesting 0 ( 1 ( 2 ... ))
	int level = 1;

	for (int i = 1; i < tl.len-1; i++) {

		if (tl.tokens[i].type == T_ATOM && tl.tokens[i].atom_len != 0) {
			list_append(root, make_ast_single(tl.tokens[i]));
		}

		else if (tl.tokens[i].type == T_OPEN_PAREN) {
			// find matching close paren

			int j = i;
			while (level > 0 && j < tl.len-1) {
				if (tl.tokens[j].type == T_OPEN_PAREN) {
					level++;
				} else if (tl.tokens[j].type == T_CLOSE_PAREN) {
					level--;
					if (level == 1) {
						// matching close paren was found, index=j
						break;
					}
				}
				j++;
			}

			// reached end without level=0 again
			if (i >= tl.len - 1) {
				printf("parse error");
				return NULL;
			}

			// now i = index of open paren
			// j = index of matching close paren

			TokenList sublist = {
				.tokens = tl.tokens + i,
				.len = j - i + 1
			};
			list_append(root, make_ast_list_simple(sublist));
			// magic
			i = j;
		}
	}

	return root;
}

ASTNode* make_ast(TokenList tl) {
	if (tl.len == 0) {
		return NULL;
	}

	if (tl.len == 1 && tl.tokens[0].type == T_ATOM) {	
		return make_ast_single(tl.tokens[0]);
	}

	else if (tl.tokens[0].type == T_OPEN_PAREN && tl.tokens[tl.len - 1].type == T_CLOSE_PAREN) {
		return make_ast_list_simple(tl);
	}

	else {
		printf("parse error\n");
		return NULL;
	}
}

// step 3: ast tree to expr tree
// step 4: collapse expr tree to get a single value

struct Expr;

typedef struct {
	char* name;
	int len;
} E_Ident;

// a value returned from the program
typedef enum {
	V_NONE,
	V_INT,
	V_LIST // list of values
} ValueType;

struct Value;

typedef struct {
	struct Value* values;
	int num_values;
} ValueList;

#define vl_new() \
	((ValueList){0})

#define vl_resize(vl, n) \
	do { \
		(vl).num_values = (n); \
		(vl).values = realloc((vl).values, sizeof(Value) * (vl).num_values); \
	} while(0)

#define vl_append(vl, ... ) \
	do { \
		vl_resize((vl), (vl).num_values + 1); \
		(vl).values[(vl).num_values - 1] = (__VA_ARGS__); \
	} while(0)

typedef struct Value {
	ValueType type;
	union {
		int int_value;
		ValueList list_value;
	};
} Value;

typedef Value E_Func(struct Expr*);

// pass this to rt_func() to signify that the function takes a variable #
// of arguments
// RTFN = runtime function
#define RTFN_VARARGS -1

// symbolic macro to show what type the "actual" varargs all are
// eg. sum-n-lists INT, VARARGS(LIST) 
#define VARARGS(T) T

/* 	associative type that holds the name and pointer to a function as well as
	# of arguments */
typedef struct {
	// in (+ 2 3) name is "+"
	char* name;
	int name_len;

	int num_args; // THIS CAN BE -1
	ValueType* arg_types; // NULL if num_args is -1
	ValueType return_type;

	E_Func* actual_function;
} E_FuncData;

typedef struct {
	E_FuncData func;
	struct Expr** args;
	int real_num_args; // THIS CANNOT BE -1
} E_FuncCall;

Value e_func_add(struct Expr* e);
Value e_func_sub(struct Expr* e);
Value e_func_mul(struct Expr* e);
Value e_func_mod(struct Expr* e);

Value e_func_eq(struct Expr* e);
Value e_func_neq(struct Expr* e);
Value e_func_lt(struct Expr* e);
Value e_func_gt(struct Expr* e);
Value e_func_le(struct Expr* e);
Value e_func_ge(struct Expr* e);

Value e_func_bool(struct Expr* e);

Value e_func_fib(struct Expr* e);

Value e_func_list(struct Expr* e);
Value e_func_len(struct Expr* e);
Value e_func_sum(struct Expr* e);
Value e_func_range(struct Expr* e);

Value e_func_if(struct Expr* e);

// list of functions defined in the runtime
typedef struct {
	E_FuncData* fns;
	int num_fns;
} RT_FnList;

#define rt_fnlist_new() \
	((RT_FnList){0})

#define rt_fnlist_resize(l, n) \
	do { \
		(l).num_fns = (n); \
		(l).fns = realloc((l).fns, sizeof(E_FuncData) * (l).num_fns); \
	} while(0)

#define rt_fnlist_append(l, ...) \
	do { \
		rt_fnlist_resize(l, (l).num_fns + 1); \
		(l).fns[(l).num_fns - 1] = (E_FuncData)__VA_ARGS__; \
	} while(0)

#define rt_fnlist_swap(l, i, j) \
	do { \
		E_FuncData temp = (l).fns[(i)]; \
		(l).fns[(i)] = (l).fns[(j)]; \
		(l).fns[(j)] = temp; \
	} while(0)

#define rt_fnlist_remove(l, index) \
	do { \
		rt_fnlist_swap(l, index, (l).num_fns - 1); \
		rt_fnlist_resize(l, (l).num_fns - 1); \
	} while(0)

// list of functions available in the runtime
// their argument count, argument types, return types are all specified in here
RT_FnList RT_BUILTIN_FUNCTIONS = {0};

typedef enum {
	E_NONE,
	E_INT,
	E_IDENT,
	E_FUNCCALL,
} ExprType;

typedef struct Expr {
	ExprType type;
	union {
		int intlit;
		E_Ident ident;
		E_FuncCall funccall;
	};
} Expr;

#define expr_new() \
	(calloc(1, sizeof(Expr)))

#define expr_print(e) \
	do { \
		expr_print_rec(e); \
		putc('\n', stdout); \
	} while(0)

void expr_print_rec(Expr* e) {
	if (e->type == E_INT) {
		printf("(int %d)", e->intlit);
	} else if (e->type == E_FUNCCALL) {
		printf("(%.*s ",
			e->funccall.func.name_len,
			e->funccall.func.name);

		for (int i = 0; i < e->funccall.real_num_args; i++) {
			expr_print_rec(e->funccall.args[i]);

			if (i != e->funccall.real_num_args - 1) {
				putc(' ', stdout);
			}

		}

		putc(')', stdout);

	} else if (e->type == E_IDENT) {
		printf("%.*s", e->ident.len, e->ident.name);
	} else {
		putc('?', stdout);
	}
}

Expr* parse(ASTNode* ast);

// ast_matches_*** should not write to out unless it will also return true

bool ast_matches_intlit(ASTNode* ast, int* out) {
	if (ast->type != A_ATOM) {
		return false;
	}

	// try parsing as int - 0 for auto-detect base

	char* end;
	int result = strtol(ast->atom_str, &end, 0);

	if (end == ast->atom_str) {
		// fail
		return false;
	}

	// success
	*out = result;
	return true;
}

bool ast_matches_ident(ASTNode* ast, E_Ident* out) {
	if (ast->type != A_ATOM) {
		return false;
	}

	out->name = ast->atom_str;
	out->len = ast->atom_len;
	return true;
}

bool ast_matches_funccall(ASTNode* ast, E_FuncCall* out) {
	if (ast->type != A_LIST
	|| ast->list_len == 0
	|| ast->list_items[0]->type != A_ATOM) {
		return false;
	}

	for (int i = 0; i < RT_BUILTIN_FUNCTIONS.num_fns; i++) {
		E_FuncData* fd = &RT_BUILTIN_FUNCTIONS.fns[i];

		if (strncmp(
			ast->list_items[0]->atom_str,
			fd->name,
			fd->name_len) != 0)
		{
			continue;
		}

		// at this point, function name matches

		int real_num_args;

		if (fd->num_args != -1) {
			real_num_args = fd->num_args;
		} else {
			real_num_args = ast->list_len - 1;
		}

		if (real_num_args != ast->list_len - 1) {
			panic("%.*s: expected %d arguments, got %d",
				ast->list_items[0]->atom_len,
				ast->list_items[0]->atom_str,
				real_num_args,
				ast->list_len - 1);
		}

		out->func = *fd;
		out->real_num_args = real_num_args;
		out->args = malloc(real_num_args * sizeof(Expr*));
		
		for (int i = 0; i < real_num_args; i++) {
			out->args[i] = parse(ast->list_items[i + 1]);
		}

		return true;
	}

	panic("unknown function \"%.*s\"",
		ast->list_items[0]->atom_len,
		ast->list_items[0]->atom_str);
}

Expr* parse(ASTNode* ast) {

	Expr* e = expr_new();

	if (ast == NULL) {
		panic("parse: ast is null");
	}

	if (ast_matches_intlit(ast, &e->intlit)) {
		e->type = E_INT;
		return e;
	}
	
	if (ast_matches_ident(ast, &e->ident)) {
		e->type = E_IDENT;
		return e;
	}

	if (ast_matches_funccall(ast, &e->funccall)) {
		e->type = E_FUNCCALL;
		return e;
	}

	panic("parse: could not match expr");
}

Value eval(Expr* e);

char* stringify_value_type(ValueType type) {
	switch (type) {
		case V_NONE: return "none";
		case V_INT: return "int";
		case V_LIST: return "list of int";
	}
}

// called outside of each e_func_***
void assert_funccall_arg_count_correct(Expr* e) {
	if (e->funccall.func.num_args == -1
	|| e->funccall.func.num_args == e->funccall.real_num_args) {
		return;
	}

	panic("%.*s, got %d arguments, expected %d",
		e->funccall.func.name_len,
		e->funccall.func.name,
		e->funccall.real_num_args,
		e->funccall.func.num_args);
}

// called inside each e_func_***, once per argument
Value try_eval_arg_as_type(Expr* e, int arg_num, ValueType type) {

	E_FuncData fd = e->funccall.func;
	Value v = eval(e->funccall.args[arg_num]);
	if (v.type != type) {
		panic("%.*s: argument %d is type %s, expected %s",
			fd.name_len,
			fd.name,
			arg_num,
			stringify_value_type(v.type),
			stringify_value_type(type));
	}

	return v;
}

// (+ (int n1) (int n2))
Value e_func_add(Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 + n1
	};
}

// (- (int n1) (int n2))
Value e_func_sub(Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 - n1
	};
}

// (* (int n1) (int n2))
Value e_func_mul(Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 * n1
	};
}

// (% (int n1) (int n2))
Value e_func_mod(Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 % n1
	};
}

// (= n1 n2)
Value e_func_eq(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 == n1
	};
}

// (!= n1 n2)
Value e_func_neq(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 != n1
	};
}

// (< n1 n2)
Value e_func_lt(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 < n1
	};
}

// (> n1 n2)
Value e_func_gt(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 > n1
	};
}

// (<= n1 n2)
Value e_func_le(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 <= n1
	};
}

// (>= n1 n2)
Value e_func_ge(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int n0 = arg0.int_value;
	int n1 = arg1.int_value;

	return (Value){
		.type = V_INT,
		.int_value = n0 >= n1
	};
}

// (bool (int x))
Value e_func_bool(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);

	int n0 = arg0.int_value;

	return (Value){
		.type = V_INT,
		.int_value = !!n0
	};
}

size_t e_func_fib_r(size_t n) {
	if (n < 2)
		return 1;
	else
		return e_func_fib_r(n-1) + e_func_fib_r(n-2);
}

// (fib n)
Value e_func_fib(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);

	int n = arg0.int_value;

	return (Value){
		.type = V_INT,
		.int_value = e_func_fib_r(n)
	};
}

// (list (int n0) (int n1) ...)
Value e_func_list(struct Expr* e) {

	ValueList result = vl_new();

	// empty list
	if (e->funccall.real_num_args == 0) {
		return (Value){
			.type = V_LIST,
			.list_value = result
		};
	}

	for (int i = 0; i < e->funccall.real_num_args; i++) {
		Value list_item = try_eval_arg_as_type(e, i, V_INT);
		vl_append(result, list_item);
	}
	
	return (Value){
		.type = V_LIST,
		.list_value = result
	};
}

// (len (list l))
Value e_func_len(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_LIST);

	int result = arg0.list_value.num_values;

	return (Value){
		.type = V_INT,
		.int_value = result
	};
}

// (sum (list l))
Value e_func_sum(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_LIST);
	
	int result = 0;

	for (int i = 0; i < arg0.list_value.num_values; i++) {

		Value* arg_value_n = &arg0.list_value.values[i];
		if (arg_value_n->type != V_INT) {
			panic("sum: argument 1 should be list of int");
		}

		result += arg_value_n->int_value;
	}

	return (Value){
		.type = V_INT,
		.int_value = result
	};
}

// (range start stop)
Value e_func_range(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);

	int start = arg0.int_value;
	int stop = arg1.int_value;

	ValueList result = vl_new();

	for (int i = start; i < stop; i++) {
		vl_append(result, (Value){
			.type = V_INT,
			.int_value = i	
		});
	}

	return (Value){
		.type = V_LIST,
		.list_value = result
	};
}

// (if cond expr1 expr2)
Value e_func_if(struct Expr* e) {

	Value arg0 = try_eval_arg_as_type(e, 0, V_INT);
	Value arg1 = try_eval_arg_as_type(e, 1, V_INT);
	Value arg2 = try_eval_arg_as_type(e, 2, V_INT);

	int if_cond = arg0.int_value;
	int then_expr = arg1.int_value;
	int else_expr = arg2.int_value;

	int result = if_cond ? then_expr : else_expr;

	return (Value){
		.type = V_INT,
		.int_value = result
	};
}

// another associative type
typedef struct {
	char* name;
	Value value;
} VarData;

typedef struct {
	VarData* vars;
	int num_vars;	
} RT_VarList;

#define rt_varlist_new() \
	((RT_VarList){0})

#define rt_varlist_resize(l, n) \
	do { \
		(l).num_vars = (n); \
		(l).vars = realloc((l).vars, sizeof(VarData) * (l).num_vars); \
	} while(0)

#define rt_varlist_append(l, ...) \
	do { \
		rt_varlist_resize(l, (l).num_vars + 1); \
		(l).vars[(l).num_vars - 1] = (VarData)__VA_ARGS__; \
	} while(0)

#define rt_varlist_swap(l, i, j) \
	do { \
		VarData temp = (l).vars[(i)]; \
		(l).vars[(i)] = (l).vars[(j)]; \
		(l).vars[(j)] = temp; \
	} while(0)

#define rt_varlist_remove(l, index) \
	do { \
		rt_fnlist_swap(l, index, (l).num_vars - 1); \
		rt_fnlist_resize(l, (l).num_vars - 1); \
	} while(0)

RT_VarList RT_CONSTANT_VARS = {0};

// a name that starts with '#'
Value eval_constant(Expr* e) {
	for (int i = 0; i < RT_CONSTANT_VARS.num_vars; i++) {
		VarData* vd = &RT_CONSTANT_VARS.vars[i];
		if (!strncmp(
			vd->name,
			e->ident.name,
			e->ident.len))
		{
			// names match
			return vd->value;
		}
	}
	panic("unknown constant %.*s", e->ident.len, e->ident.name);
}

Value eval(Expr* e) {
	if (e->type == E_INT) {
		return (Value){.type = V_INT, .int_value = e->intlit};
	}

	if (e->type == E_IDENT) {
		if (e->ident.len == 0) {
			panic("somehow ident len is 0, fix this now");
		}

		if (e->ident.name[0] == '#') {
			// constant
			return eval_constant(e);
		}
		
		panic("ident %.*s has no value yet", e->ident.len, e->ident.name);
	}

	if (e->type == E_FUNCCALL) {
		assert_funccall_arg_count_correct(e);
		return e->funccall.func.actual_function(e);
	}
}

/*
	TODO
	- make sure parens are balanced and token list is well formed
	- ast edge cases?
	- "or" and "and" functions for list of bools
	- remaining math+comparison functions
	- specific types for bool and float
	- printing types ("list of bool") or type representation
		- maybe (type mylist (list bool))
	- string types and string literals
	- runtime and variables
	- reading from a file/command line interface and help messages
	- repl mode
	- better error messages

	

	language currently supports:

	- builtin types:
		- ints, or just integer literals that fit into standard C int type
		- lists of int literals
		- TODO boolean constants #true and #false

	- constants which always start with a #, but it's not a reserved character

	- ability to do function calls with prefix notation similar to lisp
		(+ 5 6) = 11

	- builtin functions:

		- arithmetic operators, all are (int, int) -> int
			(+ x y)
			(- x y)
			(* x y)
			(% x y)

		- comparison operators, all are (int, int) -> int
			(= x y)
			(!= x y)
			(< x y)
			(<= x y)
			(> x y)
			(>= x y)

		- type conversion
			(bool x) - convert a number to 0 or 1 (equivalent to `!!x` in C)

		- list operations
			(list ...) - construct a list of ints
			(len l) - get the length of a list
			(sum l) - sum up a list of ints
			(range start stop) - construct a list of ints in range [start, stop)

		- logic
			(if cond then else)

		- other
			(fib n) - compute nth fibonacci number

*/

void rt_init();

int main() {

	rt_init();

	char* line = "(if #false 5 11)";	
	TokenList tl = tokenize(line);
	ASTNode* ast = make_ast(tl);
	Expr* e = parse(ast);
	Value result = eval(e);

	// printf("%d\n", result.list_value.num_values);
	printf("%d\n", result.int_value);
	
	return 0;
}

#define rt_add_constant(name_cstrlit, ...) \
	rt_varlist_append(RT_CONSTANT_VARS, (VarData){ \
		.name=(name_cstrlit), \
		.value=(__VA_ARGS__) \
	})

// declare a runtime function (without having to specify name len separately)
// the ... is the list of return types so you can pass like {V_INT, V_LIST, V_INT, ...}
// for varargs all of the varargs will be evaluated as the last type in the list
// and num_args should be the number of REQUIRED (aka non-vararg) arguments, 
// which can be 0
#define rt_add_func(name_cstrlit, actual_func_ptr, \
func_arg_count, func_ret_type, ...) \
	rt_fnlist_append(RT_BUILTIN_FUNCTIONS, (E_FuncData){ \
		.name = (name_cstrlit), \
		.name_len = strlen(name_cstrlit), \
		.num_args = (func_arg_count == RTFN_VARARGS \
			? RTFN_VARARGS \
			: (int) (sizeof((ValueType[]) __VA_ARGS__) / sizeof(ValueType))), \
		.arg_types = ((ValueType[]) __VA_ARGS__), \
		.return_type = func_ret_type, \
		.actual_function = (actual_func_ptr) \
	})

void rt_init() {

	RT_CONSTANT_VARS = rt_varlist_new();

	rt_add_constant("#false", (Value){.type=V_INT, .int_value=0});
	rt_add_constant("#true", (Value){.type=V_INT, .int_value=1});

	RT_BUILTIN_FUNCTIONS = rt_fnlist_new();

	rt_add_func("+", e_func_add, V_INT, 2, {V_INT, V_INT});
	rt_add_func("-", e_func_sub, V_INT, 2, {V_INT, V_INT});
	rt_add_func("*", e_func_mul, V_INT, 2, {V_INT, V_INT});
	rt_add_func("%", e_func_mod, V_INT, 2, {V_INT, V_INT});
	rt_add_func("=", e_func_eq, V_INT, 2, {V_INT, V_INT});
	rt_add_func("!=", e_func_neq, V_INT, 2, {V_INT, V_INT});
	rt_add_func(">", e_func_gt, V_INT, 2, {V_INT, V_INT});
	rt_add_func("<=", e_func_le, V_INT, 2, {V_INT, V_INT});
	rt_add_func("<", e_func_lt, V_INT, 2, {V_INT, V_INT});
	rt_add_func(">=", e_func_ge, V_INT, 2, {V_INT, V_INT});
	rt_add_func("bool", e_func_bool, V_INT, 1, {V_INT});
	rt_add_func("fib", e_func_fib, V_INT, 1, {V_INT});
	rt_add_func("list", e_func_list, V_LIST, RTFN_VARARGS, {});
	rt_add_func("len", e_func_len, V_INT, 1, {V_LIST});
	rt_add_func("sum", e_func_sum, V_INT, 1, {V_LIST});
	rt_add_func("range", e_func_range, V_LIST, 2, {V_INT, V_INT});
	rt_add_func("if", e_func_if, V_INT, 3, {V_INT, V_INT, V_INT});
}
