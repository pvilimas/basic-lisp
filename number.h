#ifndef NUMBER_H
#define NUMBER_H

#include "common.h"

typedef struct {
	enum { N_INT, N_DOUBLE } type;
	union { int int_value; double double_value; };
} Number;

// conversion and constructors

Number num_from_int(int i);
int num_to_int(Number n);

Number num_from_double(double d);
double num_to_double(Number n);

// returns false if failed, or writes to the out-param
bool num_from_string(char* str, Number* out);

// never fails
char* num_to_string(Number n);

// operators

bool num_eq(Number n1, Number n2);
bool num_lt(Number n1, Number n2);
bool num_le(Number n1, Number n2);
bool num_gt(Number n1, Number n2);
bool num_ge(Number n1, Number n2);

Number num_add(Number n1, Number n2);
Number num_sub(Number n1, Number n2);
Number num_mul(Number n1, Number n2);
Number num_div(Number n1, Number n2);

// list of number or list of list

struct List;

typedef struct {
	enum { LI_NUMBER, LI_LIST } type;
	union { Number number; struct List* list; };
} ListItem;

typedef struct List {
	ListItem* items;
	int len;
} List;

#define list_new() \
	((List){0})

#define list_resize(l, n) \
	do { \
		(l).len = (n); \
		(l).items = realloc((l).items, sizeof(Number) * (l).len); \
	} while(0)

#define list_append_number(l, ... ) \
	do { \
		list_resize((l), (l).len + 1); \
		(l).items[(l).len - 1].type = LI_NUMBER; \
		(l).items[(l).len - 1].number = __VA_ARGS__; \
	} while(0)

#define list_append_list(l, ... ) \
	do { \
		list_resize((l), (l).len + 1); \
		(l).items[(l).len - 1].type = LI_LIST; \
		(l).items[(l).len - 1].list = __VA_ARGS__; \
	} while(0)


#endif // NUMBER_H
