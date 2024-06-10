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

typedef enum {
	A_NONE,
	A_ATOM,
	A_LIST
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

struct Expr;

typedef struct {
	char* name;
	int len;
} E_Ident;

// a value returned from the program
typedef struct {
	int int_value;
} Value;

typedef Value E_Func(struct Expr*);

/* 	associative type that holds the name and pointer to a function as well as
	# of arguments */
typedef struct {
	// in (+ 2 3) name is "+"
	char* name;
	int name_len;

	int num_args;

	E_Func* actual_function;
} E_FuncData;

typedef struct {
	E_FuncData func;
	struct Expr** args;
} E_FuncCall;

Value e_func_add(struct Expr* e);
Value e_func_mul(struct Expr* e);

// list of functions available in the runtime
E_FuncData RT_FUNCTIONS[] = {
	{"+", 1, 2, e_func_add},
	{"*", 1, 2, e_func_mul},
	{0}
};

typedef enum {
	E_NONE,
	E_INTLIT,
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
	if (e->type == E_INTLIT) {
		printf("(int %d)", e->intlit);
	} else if (e->type == E_FUNCCALL) {
		printf("(%.*s ",
			e->funccall.func.name_len,
			e->funccall.func.name);

		for (int i = 0; i < e->funccall.func.num_args; i++) {
			expr_print_rec(e->funccall.args[i]);

			if (i != e->funccall.func.num_args - 1) {
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

	for (E_FuncData* fd = &RT_FUNCTIONS[0]; fd->name != NULL; fd++) {
		if (strncmp(
			ast->list_items[0]->atom_str,
			fd->name,
			fd->name_len) != 0)
		{
			continue;
		}

		// function name matches

		// +1 for fn name
		if (ast->list_len != fd->num_args + 1) {
			panic("%.*s: expected %d arguments, got %d",
				ast->list_items[0]->atom_len,
				ast->list_items[0]->atom_str,
				fd->num_args,
				ast->list_len - 1);
		}

		out->func = *fd;
		out->args = malloc(fd->num_args * sizeof(Expr*));
		for (int i = 0; i < fd->num_args; i++) {
			out->args[i] = parse(ast->list_items[i + 1]);
		}
		return true;
	}

	panic("ast_matches_funccall: function \"%.*s\" not found",
		ast->list_items[0]->atom_len,
		ast->list_items[0]->atom_str);
}

Expr* parse(ASTNode* ast) {

	Expr* e = expr_new();

	if (ast == NULL) {
		panic("parse: ast is null");
	}

	if (ast_matches_intlit(ast, &e->intlit)) {
		e->type = E_INTLIT;
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

Value e_func_add(Expr* e) {
	return (Value){
		.int_value = eval(e->funccall.args[0]).int_value
				+ eval(e->funccall.args[1]).int_value
	};
}

Value e_func_mul(Expr* e) {
	return (Value){
		.int_value = eval(e->funccall.args[0]).int_value
				* eval(e->funccall.args[1]).int_value
	};
}

Value eval(Expr* e) {
	if (e->type == E_INTLIT) {
		return (Value){.int_value = e->intlit};
	}

	if (e->type == E_IDENT) {
		panic("ident %.*s has no value yet", e->ident.len, e->ident.name);
	}

	if (e->type == E_FUNCCALL) {
		return e->funccall.func.actual_function(e);
	}
}

int main() {

	char* line = "(+ (* 9 3) 7)";
	
	TokenList tl = tokenize(line);
	// TODO make sure parens are balanced etc
	// assert_token_list_well_formed(tl);
	// tl_print(tl);

	ASTNode* ast = make_ast(tl);

	Expr* e = parse(ast);

	// expr_print(e);

	Value result = eval(e);
	printf("%d\n", result.int_value);

	return 0;
}
