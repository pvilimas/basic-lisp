#include "ast.h"

Macro BUILTIN_MACROS[] = (Macro[]){
	{"+", -1, ast_macro_add}, // adds n numbers
	{"*", -1, ast_macro_mul}, // multiplies n numbers
	{"list", -1, ast_macro_list}, // constructs a list from n numbers
	{"sum", 1, ast_macro_sum}, // sum of a list of numbers
	{"range", 2, ast_macro_range}, // constructs a list from a range
	{0}
};

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
			if (node->list.items[i].type == LI_LIST) {
				// not doing recursive prints for now
				printf("List<len=%d>\n", node->list.items[i].list->len);
				continue;
			}

			Number num = node->list.items[i].number;
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

// (list num...) -> (list num)
void ast_macro_list(ASTNode* root) {

	// (make-list) -> ([])
	if (root->nlist_len == 1) {
		root->type = A_LIST;
		root->list = list_new();
		return;
	}

	// handle varargs

	List l = list_new();
	// -1 for "make-list"
	list_resize(l, root->nlist_len - 1);

	for (int i = 1; i < root->nlist_len; i++) {
		ASTNode* arg_n = root->nlist_items[i];

		simplify_ast(arg_n);
		if (arg_n->type == A_NUMBER) {
			l.items[i - 1].type = LI_NUMBER;
			l.items[i - 1].number = arg_n->number_value;
		} else if (arg_n->type == A_LIST) {
			l.items[i - 1].type = LI_LIST;
			l.items[i - 1].list = &arg_n->list;
		}

		// expression failed to simplify or argtypes didn't match
		// TODO fix these fucking messages all over
		else {
			panic("list: argument was not a number or a list");
		}
	}

	root->type = A_LIST;
	root->list = l;
}

// (sum list) -> int
void ast_macro_sum(ASTNode* root) {

	ASTNode* list_arg = root->nlist_items[1];

	// case 1: is already a list
	if (list_arg->type == A_LIST) {

		List* l = &root->list;

		Number n = num_from_int(0);
		for (int i = 0; i < l->len; i++) {
			if (l->items[i].type != LI_NUMBER) {
				panic("sum: argument #1 must be a list of numbers");
			}
			n = num_add(n, l->items[i].number);
		}

		root->type = A_NUMBER;
		root->number_value = n;
		return;
	}

	// case 2: can "collapse" into a list
	if (list_arg->type == A_NLIST) {
		simplify_ast(list_arg);

		if (list_arg->type == A_LIST) {

			List* l = &list_arg->list;

			Number n = num_from_int(0);
			for (int i = 0; i < l->len; i++) {
				if (l->items[i].type != LI_NUMBER) {
					panic("sum: argument #1 must be a list of numbers");
				}
				n = num_add(n, l->items[i].number);
			}

			root->type = A_NUMBER;
			root->number_value = n;
			return;
		}
	}

	// otherwise it is not a list
	panic("sum: argument #1 must be of type list");

}

// (range int int) -> list
void ast_macro_range(ASTNode* root) {

	ASTNode* arg1 = root->nlist_items[1];
	ASTNode* arg2 = root->nlist_items[2];

	if (arg1->type != A_NUMBER) {
		simplify_ast(arg1);
	}

	if (arg2->type != A_NUMBER) {
		simplify_ast(arg2);
	}

	if (arg1->type != A_NUMBER || arg1->number_value.type != N_INT) {
		panic("range: argument #1 must be of type int");
	}

	if (arg2->type != A_NUMBER || arg2->number_value.type != N_INT) {
		panic("range: argument #2 must be of type int");
	}

	int start = arg1->number_value.int_value;
	int stop = arg2->number_value.int_value;

	root->type = A_LIST;
	root->list = list_new();

	for (int i = start; i < stop; i++) {
		list_append_number(root->list, num_from_int(i));
	}
}

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

