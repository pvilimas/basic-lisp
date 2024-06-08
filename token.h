#ifndef TOKEN_H
#define TOKEN_H

#include "common.h"

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

typedef struct {
	Token* tokens;
	int len;
} TokenList;

// step 1 in the program's execution
TokenList tokenize(char* prog);

// token methods

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

// tokenlist methods

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

#endif // TOKEN_H
