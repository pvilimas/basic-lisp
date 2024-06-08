#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// general

// parse error - print and exit
void panic(const char* errmsg) {
	fprintf(stderr, "error: %s\n", errmsg);
	exit(1);
}

// token

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

// tokenlist

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

// number

typedef struct {
	enum { N_INT, N_DOUBLE } type;
	union { int int_value; double double_value; };
} Number;

Number num_from_int(int i) {
	return (Number){.type = N_INT, .int_value = i};
}

int num_to_int(Number n) {
	if (n.type == N_INT) {
		return n.int_value;
	} else if (n.type == N_DOUBLE) {
		return (int) n.double_value;
	} else {
		panic("num_to_int: unrecognized numtype");
	}
}

Number num_from_double(double d) {
	return (Number){.type = N_DOUBLE, .double_value = d};
}

double num_to_double(Number n) {
	if (n.type == N_INT) {
		return (double) n.int_value;
	} else if (n.type == N_DOUBLE) {
		return n.double_value;
	} else {
		panic("num_to_double: unrecognized numtype");
	}
}

// returns false if failed, or puts in the out-param
bool num_from_string(char* str, Number* out) {
	char** endptr;

	// try converting to int (0 = auto detect base)
	long l = strtol(str, endptr, 0);

	if (*endptr != str) {
		// success
		*out = num_from_int((int)l);
		return true;
	}

	// failure, try converting to double
	double d = strtod(str, endptr);

	if (*endptr != str) {
		// success
		*out = num_from_double(d);
		return true;
	}
	
	return false;
}

char* num_to_string(Number n) {
	char* buf;
	int len;
	if (n.type == N_INT) {
		len = snprintf(NULL, 0, "%d", n.int_value);
		buf = malloc(len + 1);
		snprintf(buf, len, "%d", n.int_value);
	} else if (n.type == N_DOUBLE) {
		len = snprintf(NULL, 0, "%lf", n.double_value);
		buf = malloc(len + 1);
		snprintf(buf, len, "%lf", n.double_value);
	}

	return buf;
}

bool num_eq(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return n1.int_value == n2.int_value;
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return (double)n1.int_value == n2.double_value;
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return n1.double_value == (double)n2.int_value;
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return n1.double_value == n2.double_value;
	} else {
		panic("num_eq: unknown state");
	}
}

bool num_lt(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return n1.int_value < n2.int_value;
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return (double)n1.int_value < n2.double_value;
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return n1.double_value < (double)n2.int_value;
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return n1.double_value < n2.double_value;
	} else {
		panic("num_lt: unknown state");
	}
}

bool num_gt(Number n1, Number n2) {
	return num_lt(n2, n1);
}

bool num_le(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return n1.int_value <= n2.int_value;
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return (double)n1.int_value <= n2.double_value;
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return n1.double_value <= (double)n2.int_value;
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return n1.double_value <= n2.double_value;
	} else {
		panic("num_lt: unknown state");
	}
}

bool num_ge(Number n1, Number n2) {
	return num_le(n2, n1);
}

Number num_add(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return num_from_int(n1.int_value + n2.int_value);
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return num_from_double((double)n1.int_value + n2.double_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return num_from_double(n1.double_value + (double)n2.int_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return num_from_double(n1.double_value + n2.double_value);
	} else {
		panic("num_add: unknown state");
	}
}

Number num_sub(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return num_from_int(n1.int_value - n2.int_value);
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return num_from_double(n1.int_value - n2.double_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return num_from_double(n1.double_value - n2.int_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return num_from_double(n1.double_value - n2.double_value);
	} else {
		panic("num_sub: unknown state");
	}
}

Number num_mul(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return num_from_int(n1.int_value * n2.int_value);
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return num_from_double(n1.int_value * n2.double_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return num_from_double(n1.double_value * n2.int_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return num_from_double(n1.double_value * n2.double_value);
	} else {
		panic("num_mul: unknown state");
	}
}

Number num_div(Number n1, Number n2) {
	if (n1.type == N_INT && n2.type == N_INT) {
		return num_from_int(n1.int_value * n2.int_value);
	} else if (n1.type == N_INT && n2.type == N_DOUBLE) {
		return num_from_double(n1.int_value * n2.double_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_INT) {
		return num_from_double(n1.double_value * n2.int_value);
	} else if (n1.type == N_DOUBLE && n2.type == N_DOUBLE) {
		return num_from_double(n1.double_value * n2.double_value);
	} else {
		panic("num_mul: unknown state");
	}
}

// "real" list

typedef struct {
	Number* items;
	int len;
} List;

#define list_new() \
	((List){0})

#define list_resize(l, n) \
	do { \
		(l).len = (n); \
		(l).items = realloc((l).items, sizeof(Number) * (l).len); \
	} while(0)

#define list_append(l, ... ) \
	do { \
		list_resize((l), (l).len + 1); \
		(l).items[(l).len - 1] = (__VA_ARGS__); \
	} while(0)

// ast

// starting options: atom and nlist indicate that a node hasn't been fully
// processed yet, after that happens it gets converted to a "real" type
typedef enum {
	A_NONE,
	A_ATOM, 		// a generic string - fallback/starting option
	A_NLIST,		// a list of 0 or more ASTNode - fallback/starting option

	// real types
	A_NUMBER,		// a number
	A_LIST			// a fixed-size array of 0 or more numbers (later lists too)
} ASTType;

typedef struct ASTNode {
	ASTType type;
	union {
		// A_ATOM
		struct { char* atom_str; int atom_len; };
		// A_NLIST
		struct { struct ASTNode** nlist_items; int nlist_len; };
		// A_NUMBER
		Number number_value;
		// A_LIST
		List list;
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
	} else if (node->type == A_NUMBER) {
		if (node->number_value.type == N_INT) {
			printf("ASTNode<type=A_NUMBER (int), %d>\n",
				node->number_value.int_value);
		} else if (node->number_value.type == N_DOUBLE) {
			printf("ASTNode<type=A_NUMBER (double), %lf>\n",
				node->number_value.double_value);
		}
	} else if (node->type == A_LIST) {
		printf("ASTNode<type=A_LIST, %d items>:\n",
			node->nlist_len);
		for (int i = 0; i < node->list.len; i++) {
			Number num = node->list.items[i];
			if (num.type == N_INT) {
				printf("Number<int, %d>\n", num.int_value);
			} else if (num.type == N_DOUBLE) {
				printf("Number<double, %lf>\n", num.double_value);
			}
		}
	} else if (node->type == A_NLIST) {
		printf("ASTNode<type=A_NLIST, %d nodes>:\n",
			node->nlist_len);
		for (int i = 0; i < node->nlist_len; i++) {
			node_print_rec(node->nlist_items[i], level + 1);
		}
	} else {
		printf("ASTNode<type=A_UNIMPLEMENTED: see line %d>\n", __LINE__);
	}
}

#define node_print(node) \
	(node_print_rec((node), 0))

#define nlist_resize(node, n) \
	do { \
		(node)->nlist_len = (n); \
		(node)->nlist_items = realloc((node)->nlist_items, sizeof(ASTNode*) * (node)->nlist_len); \
	} while(0)

// arg must be a ASTNode*
#define nlist_append(node, ...) \
	do { \
		nlist_resize((node), (node)->nlist_len + 1); \
		(node)->nlist_items[(node)->nlist_len - 1] = (__VA_ARGS__); \
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
	root->type = A_NLIST;

	// level of nesting 0 ( 1 ( 2 ... ))
	int level = 1;

	for (int i = 1; i < tl.len-1; i++) {

		if (tl.tokens[i].type == T_ATOM && tl.tokens[i].atom_len != 0) {
			nlist_append(root, make_ast_single(tl.tokens[i]));
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
				panic("unbalanced parentheses");
			}

			// now i = index of open paren
			// j = index of matching close paren

			TokenList sublist = {
				.tokens = tl.tokens + i,
				.len = j - i + 1
			};
			nlist_append(root, make_ast_list_simple(sublist));
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
		panic("no matching parenthesis found");
	}
}

void simplify_ast(ASTNode* ast);

// assumes (+ n1 n2 ... ), all A_INT
// replaces this with (<sum>)
void ast_macro_add(ASTNode* root) {

	// (+) -> (0)
	if (root->nlist_len == 1) {
		root->type = A_NUMBER;
		root->number_value = num_from_int(0);
		return;
	}

	// (+ x) -> (x)
	else if (root->nlist_len == 2) {
		ASTNode* arg_n1 = root->nlist_items[1];

		// try to simplify down to an int
		simplify_ast(arg_n1);
		if (arg_n1->type == A_NUMBER) {
			root->type = A_NUMBER;
			root->number_value = arg_n1->number_value;
		}

		// TODO somewhere in here, differentiate between type mismatch and
		// parse error (once floats and other num types are added)

		// expression failed to simplify
		else {
			panic("failed to simplify expression to a single value");
		}
		return;
	}

	// handle varargs

	Number sum = num_from_int(0);
	for (int i = 1; i < root->nlist_len; i++) {
		ASTNode* arg_n = root->nlist_items[i];
		
		// try to simplify down to an int
		simplify_ast(arg_n);
		if (arg_n->type == A_NUMBER) {
			sum = num_add(sum, arg_n->number_value);
		}

		// expression failed to simplify
		else {
			panic("failed to simplify expression to a single value");
		}
	}

	root->type = A_NUMBER;
	root->number_value = sum;
}

// assumes (* n1 n2 ... ), all A_INT
// replaces this with (<product>)
void ast_macro_mul(ASTNode* root) {

	// (*) -> (1)
	if (root->nlist_len == 1) {
		root->type = A_NUMBER;
		root->number_value = num_from_int(1);
		return;
	}

	// (* x) -> (x)
	else if (root->nlist_len == 2) {
		ASTNode* arg_n1 = root->nlist_items[1];

		// try to simplify down to an int
		simplify_ast(arg_n1);
		if (arg_n1->type == A_NUMBER) {
			root->type = A_NUMBER;
			root->number_value = arg_n1->number_value;
		}

		// expression failed to simplify
		else {
			panic("failed to simplify expression to a single value");
		}
		return;
	}

	// handle varargs

	Number product = num_from_int(1);
	for (int i = 1; i < root->nlist_len; i++) {
		ASTNode* arg_n = root->nlist_items[i];
		
		// try to simplify down to an int
		simplify_ast(arg_n);
		if (arg_n->type == A_NUMBER) {
			product = num_mul(product, arg_n->number_value);
		}

		// expression failed to simplify
		else {
			panic("failed to simplify expression to a single value");
		}
	}

	root->type = A_NUMBER;
	root->number_value = product;
}

// list constructor
// assumes (list n1 n2 ...)
// replaces with ([n1 n2 ...])
void ast_macro_list(ASTNode* root) {

	// (list) -> ([])
	if (root->nlist_len == 1) {
		root->type = A_LIST;
		root->list = list_new();
		return;
	}

	// handle varargs

	List l = list_new();
	// -1 for "list"
	list_resize(l, root->nlist_len - 1);
	
	for (int i = 1; i < root->nlist_len; i++) {
		ASTNode* arg_n = root->nlist_items[i];

		simplify_ast(arg_n);
		if (arg_n->type == A_NUMBER) {
			l.items[i - 1] = arg_n->number_value;
		} else if (arg_n->type == A_LIST) {
			panic("no nested lists allowed yet");
		}

		// expression failed to simplify or argtypes didn't match
		// TODO fix these fucking messages all over
		else {
			panic("failed to simplify expression to a single value");
		}
	}

	root->type = A_LIST;
	root->list = l;
}

typedef struct {
	char* internal_name; // in the program
	int num_args; // -1 for varargs
	void (*fn)(ASTNode*);
} Macro;

Macro BUILTIN_MACROS[] = {
	{"+", -1, ast_macro_add},
	{"*", -1, ast_macro_mul},
	{"list", -1, ast_macro_list},
	{0}
};

// simplify the whole thing down by executing different macros,
// replacing in-place
void simplify_ast(ASTNode* root) {
	if (root->type == A_NUMBER) {
		// nothing to do, already simplified
		return;
	} else if (root->type == A_ATOM) {
		// try to convert to number
		if (num_from_string(strndup(root->atom_str, root->atom_len), &root->number_value)) {
			root->type = A_NUMBER;
			return;
		}

		// TODO
		// number conv failed, try to do a variable lookup or something idk
	} else if (root->type == A_NLIST) {

		// case 0: empty list ()
		if (root->nlist_len == 0) {
			return;
		}
	
		// case 1: macro call (m-name arg0 arg1 arg2 ... )
		// macros can have 0 arguments so this must be done before case 2
		// this ONLY CHECKS ARGUMENT COUNT, not the types
		// the typechecking should be done in whichever m->fn()

		if (root->nlist_items[0]->type == A_ATOM) {
			for (Macro* m = &BUILTIN_MACROS[0]; !!m->internal_name; m++) {

				bool name_matches = (strncmp(m->internal_name,
					root->nlist_items[0]->atom_str,
					root->nlist_items[0]->atom_len) == 0);
				// +1 for m-name
				bool argcount_matches = (root->nlist_len == m->num_args + 1);
				bool ignore_argcount = (m->num_args == -1);

				bool all_good = name_matches &&
					(argcount_matches || ignore_argcount);

				if (!all_good) {
					continue;
				}

				// execute the macro to modify the syntax tree
				m->fn(root);
				return;
			}
		}

		// no macro was executed

		panic("failed to simplify expression to a single value");
	} else {
		panic("fix code pls");
	}
}

int main() {
	char* prog = "(list (* 4 1) (* 5 10) (* 6 100) (* 7 1000) (* 8 10000))";
	
	TokenList tl = tokenize(prog);
	// TODO make sure parens are balanced etc
	// assert_token_list_well_formed(tl);
	// tl_print(tl);
	ASTNode* ast = make_ast(tl);
	simplify_ast(ast);
	node_print(ast);

	return 0;
}
