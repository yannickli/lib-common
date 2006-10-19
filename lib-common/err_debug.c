/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef NDEBUG

#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <ctype.h>

#include "macros.h"
#include "mem.h"
#include "err_report.h"
#include "string_is.h"

int e_verbosity_level = 0;

#define DEBUG_HASH_SIZE 256

typedef struct h_entry {
    struct h_entry *next;
    uint32_t hash;
    const char data[];
} h_entry;

static h_entry *e_files[DEBUG_HASH_SIZE];
static h_entry *e_funcs[DEBUG_HASH_SIZE];

static inline uint32_t hash(uint32_t init, const char *p) {
    uint32_t result = init;

    while (*p) {
        result  = (result << 3) | (result >> (32-3));
        result += *p++;
    }

    return result;
}

static void e_debug_initialize(void)
{
    static bool initialized = false;
    const char *p;

    if (initialized)
        return;

    p = getenv("IS_DEBUG");

    while (p && *p) {
        const char *q = p;
        bool seen_slash = false;
        h_entry *e;
        h_entry **list;

        while (isspace(*p)) {
            p++;
        }

        while (*q && !isspace(*q)) {
            seen_slash |= *q++ == '/';
        }

        e = mem_alloc0(sizeof(h_entry) + q - p + 1);

        memcpy(&e->data, p, q - p);
        e->hash = hash(0, e->data);

        list = &(seen_slash ? e_funcs : e_files)[e->hash % DEBUG_HASH_SIZE];
        e->next = *list;
        *list   = e;

        p = q;
    }

    initialized = true;
}

bool e_debug_is_watched(const char *fname, const char *func)
{
    uint32_t hash1 = hash(0, fname);
    uint32_t hash2 = hash(hash(hash1, "/"), func);
    h_entry *e;

    e_debug_initialize();

    for (e = e_files[hash1 % DEBUG_HASH_SIZE]; e; e = e->next) {
        if (e->hash == hash1 && strequal(e->data, fname))
            return true;
    }

    {
        char buf[PATH_MAX];
        snprintf(buf, sizeof(buf), "%s/%s", fname, func);

        for (e = e_funcs[hash2 % DEBUG_HASH_SIZE]; e; e = e->next) {
            if (e->hash == hash2 && strequal(e->data, buf))
                return true;
        }
    }

    return false;
}


void e_set_verbosity(int max_debug_level)
{
    e_verbosity_level = max_debug_level;
}

void e_incr_verbosity(void)
{
    e_verbosity_level++;
}

#endif
