#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define panic(...) \
    do { \
        fprintf(stderr, "error: %s\n", __VA_ARGS__); \
        exit(1); \
    } while(0)

#endif // COMMON_H
