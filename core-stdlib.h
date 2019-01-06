/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2019 INTERSEC SA                                   */
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

void ha_srand(void) __leaf;
uint32_t ha_rand(void) __leaf;
int ha_rand_range(int first, int last) __leaf;
double ha_rand_ranged(double first, double last) __leaf;

#define HA_UUID_HEX_LEN  (32 + 4)
typedef uint8_t ha_uuid_t[16];
void ha_uuid_generate_v4(ha_uuid_t uuid);
#ifndef __cplusplus
void __ha_uuid_fmt(char buf[static HA_UUID_HEX_LEN], ha_uuid_t uuid);
#else
void __ha_uuid_fmt(char buf[], ha_uuid_t uuid);
#endif
void sb_add_uuid(struct sb_t *sb, ha_uuid_t uuid);


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
