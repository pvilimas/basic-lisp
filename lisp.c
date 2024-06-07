#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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
	
	char* atom_str;
	int atom_str_len;

	int number_value_int;

	struct ASTNode** list_items;
	int list_len;
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
			node->atom_str_len,
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
	root->atom_str_len = t.atom_len;
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

typedef enum {
	R_NONE,
	R_PARSE_ERROR,
	R_INT
} ResultType;

typedef struct {
	ResultType type;
	union {	
		int int_value;
		struct { char* data; int len; } string_value;
	};
} Result;

// a function that can be used inside eval
typedef struct {
	const char* name;
	int n_args; // -1 for variable
	Result (*fn)(ASTNode*);
} ExecutionFn;

// macros that manipulate the ast
typedef struct {
	const char* name;
	int n_args;
	void (*fn)(ASTNode**);
} Macro;

Result execute_ast(ASTNode* ast);

// just int for now
// remember 0 is the function name, 1 is the first arg
Result parse_argument_as_number(ASTNode* ast, int argnum) {
	if (ast->list_items[argnum]->type == A_LIST) {
		Result r1 = execute_ast(ast->list_items[argnum]);
		if (r1.type == R_PARSE_ERROR) {
			return r1;
		}

		return r1;
	} else {
		return (Result) {
			.type = R_INT,
			.int_value = atoi(ast->list_items[argnum]->atom_str)
		};
	}
}

bool ast_matches_macro(ASTNode* ast, Macro* m) {
	if (ast->type != A_LIST)
		return false;

	if (strncmp(m->name,
	ast->list_items[0]->atom_str,
	ast->list_items[0]->atom_str_len) != 0)
		return false;

	if (m->n_args == -1)
		return true;

	if (ast->list_len != m->n_args + 1)
		return false;

	return true;
}

void try_execute_macro(ASTNode** ast, Macro* m) {
	if (ast_matches_macro(*ast, m)) {
		m->fn(ast);
	}
}

// checks if an ast represents a valid expression at the root or a valid function call (fn x y z (...) ... ) AND matches the given fn
// ef_***() will assume this has returned true for that ef type
bool ast_matches_fn(ASTNode* ast, ExecutionFn* ef) {
	if (ast->type != A_LIST)
		return false;

	if (strncmp(ef->name,
	ast->list_items[0]->atom_str,
	ast->list_items[0]->atom_str_len) != 0)
		return false;

	if (ef->n_args == -1)
		return true;

	if (ast->list_len != ef->n_args + 1)
		return false;

	return true;
}

// (+ x y z ...)
Result ef_add(ASTNode* ast) {

	int sum = 0;
	for (int i = 0; i < ast->list_len; i++) {
		Result r = parse_argument_as_number(ast, i);
		if (r.type == R_PARSE_ERROR) {
			return r;
		}

		sum += r.int_value;
	}

	return (Result) {
		.type = R_INT,
		.int_value = sum	
	};
}

// (- n1 n2)
Result ef_sub(ASTNode* ast) {
	int n1;
	int n2;

	Result r1 = parse_argument_as_number(ast, 1);
	Result r2 = parse_argument_as_number(ast, 2);

	if (r1.type == R_PARSE_ERROR || r2.type == R_PARSE_ERROR) {
		return (Result){.type=R_PARSE_ERROR};
	}

	n1 = r1.int_value;
	n2 = r2.int_value;

	return (Result){
		.type = R_INT,
		.int_value = n1 - n2
	};
}

// (* x y z ...)
Result ef_mul(ASTNode* ast) {

	int product = 0;
	for (int i = 0; i < ast->list_len; i++) {
		Result r = parse_argument_as_number(ast, i);
		if (r.type == R_PARSE_ERROR) {
			return r;
		}

		product *= r.int_value;
	}

	return (Result) {
		.type = R_INT,
		.int_value = product
	};
}

// generate random number
// (rand min max)
Result ef_rand(ASTNode* ast) {

	int min;
	int max;

	Result r_min = parse_argument_as_number(ast, 1);
	Result r_max = parse_argument_as_number(ast, 2);

	if (r_min.type == R_PARSE_ERROR || r_max.type == R_PARSE_ERROR) {
		return (Result){.type=R_PARSE_ERROR};
	}

	min = r_min.int_value;
	max = r_max.int_value;

	return (Result){
		.type = R_INT,
		.int_value = min + rand() % (max - min)
	};
}

// base level functions defined in C
// you can detect the end of the list when ef.name == NULL
ExecutionFn EF_LIST[] = {
	{"+", -1, ef_add},
	{"-", 2, ef_sub},
	{"*", -1, ef_mul},
	{"rand", 2, ef_rand},
	{0}
};

// macro that turns (inc x) into (+ x 1)
void m_inc(ASTNode** ast) {

	Result r = parse_argument_as_number(*ast, 1);

	if (r.type == R_PARSE_ERROR) {
		return;
	}

	// newly allocated, TODO free all these
	ASTNode* item0 = node_new();
	item0->type = A_ATOM;
	item0->atom_str = "+";
	item0->atom_str_len = 1;

	// not newly allocated
	ASTNode* item1 = (*ast)->list_items[1];

	ASTNode* item2 = node_new();
	item2->type = A_ATOM;
	item2->atom_str = "1";
	item2->atom_str_len = 1;

	// TODO actually free all the sub-nodes lol, write a function for this
	free((*ast)->list_items);

	(*ast)->list_items = malloc(3 * sizeof(ASTNode*));
	(*ast)->list_len = 3;
	(*ast)->list_items[0] = item0;
	(*ast)->list_items[1] = item1;
	(*ast)->list_items[2] = item2;
}

// macro that turns (range 3 7) into (3 4 5 6)
// it does modify the old node
void m_range(ASTNode** ast) {

	int start;
	int stop;

	Result r_start = parse_argument_as_number(*ast, 1);
	Result r_stop = parse_argument_as_number(*ast, 2);

	if (r_start.type == R_PARSE_ERROR || r_stop.type == R_PARSE_ERROR) {
		return;
	}

	start = r_start.int_value;
	stop = r_stop.int_value;

	free((*ast)->list_items);
	(*ast)->list_len = stop - start;
	(*ast)->list_items = malloc((*ast)->list_len * sizeof(ASTNode*));

	char buf[20];
	int index = 0;
	for (int i = start; i < stop; i++) {
		snprintf(buf, 20, "%d", i);
	
		ASTNode* item = node_new();
		item->type = A_ATOM;
		item->atom_str = strdup(buf);
		item->atom_str_len = strlen(buf);

		(*ast)->list_items[index] = item;
		index++;
	}
}

Macro M_LIST[] = {
	{"inc", 1, m_inc},
	{"range", 2, m_range},
	{0}
};

Result execute_ast(ASTNode* ast) {

	if (ast == NULL) {
		return (Result) {
			.type = R_PARSE_ERROR
		};
	}

	if (ast->type == A_LIST) {
		// check macros
		for (Macro* m = &M_LIST[0]; !!m->name; m++) {
			try_execute_macro(&ast, m);
		}

		// check functions
		for (ExecutionFn* ef = &EF_LIST[0]; !!ef->name; ef++) {
			if (ast_matches_fn(ast, ef)) {
				return ef->fn(ast);
			}
		}
	}

	return (Result){.type=R_PARSE_ERROR}; 
}

int main() {
	srand(time(0));
	// char* prog = "(sum (* 4 1) (* 5 10) (* 6 100) (* 7 1000))"; // =654
	// char* prog = "(+ 1 (sum 5 6 7 8 9))";
	// char* prog = "(rand 0 1000)";
	char* prog = "(range 1 11)";
	
	TokenList tl = tokenize(prog);
	// TODO make sure parens are balanced etc
	// assert_token_list_well_formed(tl);
	// tl_print(tl);
	ASTNode* ast = make_ast(tl);

	node_print(ast);
	Result r = execute_ast(ast);
	node_print(ast);

	if (r.type == R_PARSE_ERROR) {
		printf("parse error\n");
	} else {
		printf("Type: %d\n", r.type);
		printf("Value: %d\n", r.int_value);
	}
	
	return 0;
}
