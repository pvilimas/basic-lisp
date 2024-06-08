#include "lisp.h"

int main() {
	char* prog = "(+ 5 6)";
	
	TokenList tl = tokenize(prog);
	ASTNode* ast = make_ast(tl);
	simplify_ast(ast);
	node_print(ast);

	return 0;
}
