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
	A_ATOM, 		// a generic string, kind of a fallback option
	A_INT,			// an integer literal
	A_LIST,			// a list of 0 or more ASTNode
	A_PARSE_ERROR 	// propagates
} ASTType;

typedef struct ASTNode {
	ASTType type;
	union {
		// A_ATOM
		struct { char* atom_str; int atom_len; };
		// A_INT
		int int_value;
		// A_LIST
		struct { struct ASTNode** list_items; int list_len; };
		// A_PARSE_ERROR
		char* errmsg;
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
	} else if (node->type == A_INT) {
		printf("ASTNode<type=A_INT, %d>\n",
			node->int_value);
	} else if (node->type == A_LIST) {
		printf("ASTNode<type=A_LIST, %d items>:\n",
			node->list_len);
		for (int i = 0; i < node->list_len; i++) {
			node_print_rec(node->list_items[i], level + 1);
		}
	} else if (node->type == A_PARSE_ERROR) {
		printf("ASTNode<type=A_PARSE_ERROR, msg=\"%s\">\n",
			node->errmsg);
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

void simplify_ast(ASTNode** ast);

// assumes (+ n1 n2 ... ), all A_INT
// replaces this with (<sum>)
void ast_macro_add(ASTNode** ast) {

	// (+) -> (0)
	if ((*ast)->list_len == 1) {
		(*ast)->type = A_INT;
		(*ast)->int_value = 0;
		return;
	}

	// (+ x) -> (x)
	else if ((*ast)->list_len == 2) {
		ASTNode** arg_n1 = &(*ast)->list_items[1];

		// try to simplify down to an int
		simplify_ast(arg_n1);
		if ((*arg_n1)->type == A_INT) {
			(*ast)->type = A_INT;
			(*ast)->int_value = (*arg_n1)->int_value;
		}

		// expression failed to simplify
		else {
			(*ast)->type = A_PARSE_ERROR;
		}
		return;
	}

	// handle varargs

	int sum = 0;
	for (int i = 1; i < (*ast)->list_len; i++) {
		ASTNode** arg_n = &(*ast)->list_items[i];
		
		// try to simplify down to an int
		simplify_ast(arg_n);
		if ((*arg_n)->type == A_INT) {
			sum += (*arg_n)->int_value;
		}

		// expression failed to simplify
		else {
			(*ast)->type = A_PARSE_ERROR;
			return;
		}
	}

	(*ast)->type = A_INT;
	(*ast)->int_value = sum;
}

// assumes (* n1 n2 ... ), all A_INT
// replaces this with (<product>)
void ast_macro_mul(ASTNode** ast) {

	// (*) -> (1)
	if ((*ast)->list_len == 1) {
		(*ast)->type = A_INT;
		(*ast)->int_value = 1;
		return;
	}

	// (* x) -> (x)
	else if ((*ast)->list_len == 2) {
		ASTNode** arg_n1 = &(*ast)->list_items[1];

		// try to simplify down to an int
		simplify_ast(arg_n1);
		if ((*arg_n1)->type == A_INT) {
			(*ast)->type = A_INT;
			(*ast)->int_value = (*arg_n1)->int_value;
		}

		// expression failed to simplify
		else {
			(*ast)->type = A_PARSE_ERROR;
		}
		return;
	}

	// handle varargs

	int product = 1;
	for (int i = 1; i < (*ast)->list_len; i++) {
		ASTNode** arg_n = &(*ast)->list_items[i];
		
		// try to simplify down to an int
		simplify_ast(arg_n);
		if ((*arg_n)->type == A_INT) {
			product *= (*arg_n)->int_value;
		}

		// expression failed to simplify
		else {
			(*ast)->type = A_PARSE_ERROR;
			return;
		}
	}

	(*ast)->type = A_INT;
	(*ast)->int_value = product;
}

typedef struct {
	char* internal_name; // in the program
	int num_args; // -1 for varargs
	void (*fn)(ASTNode**);
} Macro;

Macro BUILTIN_MACROS[] = {
	{"+", -1, ast_macro_add},
	{"*", -1, ast_macro_mul},
	{0}
};

// simplify the whole thing down by executing different macros, replacing in-place
void simplify_ast(ASTNode** ast) {
	ASTNode* root = *ast;

	if (root->type == A_INT) {
		// nothing to do, already simplified
		return;
	} else if (root->type == A_ATOM) {
		// try to convert to number
		bool is_num = true;
		for (int i = 0; i < root->atom_len; i++) {
			if (!isdigit(root->atom_str[i])) {
				is_num = false;
				break;
			}
		}
		if (is_num) {
			root->type = A_INT;
			root->int_value = atoi(root->atom_str);
			return;
		}

		// number conv failed, try to do a variable lookup i guess?
		// other stuff here
		// TODO
	} else if (root->type == A_LIST) {

		// case 0: empty list ()
		if (root->list_len == 0) {
			return;
		}
	
		// case 1: macro call (m-name arg0 arg1 arg2 ... )
		// macros can have 0 arguments so this must be done before case 2

		if (root->list_items[0]->type == A_ATOM) {
			for (Macro* m = &BUILTIN_MACROS[0]; !!m->internal_name; m++) {

				bool name_matches = (strncmp(m->internal_name,
					root->list_items[0]->atom_str,
					root->list_items[0]->atom_len) == 0);
				// +1 for m-name
				bool argcount_matches = (root->list_len == m->num_args + 1);
				bool ignore_argcount = (m->num_args == -1);

				bool all_good = name_matches &&
					(argcount_matches || ignore_argcount);

				if (!all_good) {
					continue;
				}

				// execute the macro to modify the syntax tree
				m->fn(&root);
				return;
			}
		}

		root->type = A_PARSE_ERROR;
		root->errmsg = "idk lol";
	}
}

int main() {
	char* prog = "(+ (* 4 1) (* 5 10) (* 6 100) (* 7 1000))";
	
	TokenList tl = tokenize(prog);
	// TODO make sure parens are balanced etc
	// assert_token_list_well_formed(tl);
	// tl_print(tl);
	ASTNode* ast = make_ast(tl);
	simplify_ast(&ast);
	node_print(ast);

	return 0;
}
