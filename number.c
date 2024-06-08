#include "number.h"

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
