/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_CORE_STDLIB_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_CORE_STDLIB_H

/*----- core-havege.c -----*/

struct sb_t;

int is_rand(void) __leaf;

/** Thread-safe implementation of rand() using per-thread autogenerated seed.
 */
#define rand()  is_rand()

#define rands()  rands_uneeded()

/** Generate a 32bit random integer.
 */
uint32_t rand32(void) __leaf;

/** Generate a 64bit random integer.
 */
uint64_t rand64(void) __leaf;

/** Generate a random integer in the range [first,last].
 *
 * Both boundaries are included in the range of integers that can be
 * generated.
 */
int64_t rand_range(int64_t first, int64_t last) __leaf;

/** Generate a random double in the range [first,last].
 */
double rand_ranged(double first, double last) __leaf;

#define UUID_HEX_LEN  (32 + 4)
typedef uint8_t uuid_t[16];
void rand_generate_uuid_v4(uuid_t uuid);
#ifndef __cplusplus
void __uuid_fmt(char buf[static UUID_HEX_LEN], uuid_t uuid);
#else
void __uuid_fmt(char buf[], uuid_t uuid);
#endif
void sb_add_uuid(struct sb_t *sb, uuid_t uuid);


/*----- versions -----*/

typedef struct core_version_t {
    bool        is_main_version; /* Main versions are printed first */
    const char *name;
    const char *version;
    const char *git_revision;
} core_version_t;
extern core_version_t core_versions_g[8];
extern int core_versions_nb_g;

void core_push_version(bool is_main_version, const char *name,
                       const char *version, const char *git_revision);

#endif
