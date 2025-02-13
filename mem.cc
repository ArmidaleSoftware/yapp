// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

/*
 * Basic utilities.
 */

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

static void *
notnull(void *ptr)
{
    if (ptr == NULL) {
        perror("out of memory");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *
emalloc(const size_t size)
{
    if (size == 0) {
        fprintf(stderr, "emalloc(0): empty alloc\n");
        exit(EXIT_FAILURE);
    }
    return notnull(malloc(size));
}

void *
emalloz(const size_t size)
{
    void *ptr = emalloc(size);
    memset(ptr, 0, size);
    return ptr;
}

void *
erealloc(void *ptr, size_t size)
{
    if (size == 0) {
        fprintf(stderr, "erealloc(0): empty alloc\n");
        exit(EXIT_FAILURE);
    }
    return notnull(realloc(ptr, size));
}

char *
estrdup(const char *str)
{
    return (char *)notnull(strdup(str));
}

char *
estrndup(const char *str, const size_t len)
{
    return (char *)notnull(strndup(str, len));
}
