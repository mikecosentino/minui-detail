// A CHECK macro shared by the unit tests. Host-compiled by `make test`; no SDL.
#ifndef CHECK_H
#define CHECK_H

#include <stdio.h>

static int failures = 0;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        if (!(cond))                                                         \
        {                                                                    \
            fprintf(stderr, "%s:%d: FAIL: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#endif
