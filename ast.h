#ifndef AST_H
#define AST_H

#include "common.h"
#include "token.h"
#include "number.h"

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

typedef struct {
	char* internal_name; // in the program
	int num_args; // -1 for varargs
	void (*fn)(ASTNode*);
} Macro;

void ast_macro_add(ASTNode* root);
void ast_macro_mul(ASTNode* root);
void ast_macro_list(ASTNode* root);
void ast_macro_sum(ASTNode* root);
void ast_macro_range(ASTNode* root);

extern Macro BUILTIN_MACROS[];

#define node_new() \
	(calloc(1, sizeof(ASTNode)))

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
ASTNode* make_ast_single(Token t);

// assumes OPEN_PAREN, ..., CLOSE_PAREN
ASTNode* make_ast_list_simple(TokenList tl);

// step 2
ASTNode* make_ast(TokenList tl);

// step 3
void simplify_ast(ASTNode* ast);

// helper function
void node_print_rec(ASTNode* node, int level);

#endif // AST_H
