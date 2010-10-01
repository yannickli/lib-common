/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_STR_H) || defined(IS_LIB_COMMON_STR_L_H)
#  error "you must include str.h instead"
#else
#define IS_LIB_COMMON_STR_L_H

typedef struct lstr_t {
    char *s;
    int   len;
} lstr_t;

typedef struct clstr_t {
    const char *s;
    int         len;
} clstr_t;

#define LSTR_INIT(s_, len_)   { .s = (s_), .len = (len_) }
#define LSTR_INIT_V(s, len)   (lstr_t)LSTR_INIT(s, len)
#define LSTR_IMMED(str)       LSTR_INIT(str, sizeof(str) - 1)
#define LSTR_IMMED_V(str)     LSTR_INIT_V(str, sizeof(str) - 1)
#define LSTR_STR_V(str)       ({ char *__s = (str); LSTR_INIT_V(__s, strlen(__s)); })
#define LSTR_NULL             LSTR_INIT(NULL, 0)
#define LSTR_NULL_V           LSTR_INIT_V(NULL, 0)
#define LSTR_SB(sb)           LSTR_INIT((sb)->data, (sb)->len)
#define LSTR_SB_V(sb)         LSTR_INIT_V((sb)->data, (sb)->len)

#define T_LSTR_DUP(str, len)  ({ int __len = (len); LSTR_INIT_V(t_dupz(str, __len), __len); })
#define T_LSTR_DUP2(str)      ({ const char *__s = (str); T_LSTR_DUP(__s, strlen(__s)); })

static inline lstr_t lstr_dup(const lstr_t s)
{
    return LSTR_INIT_V(p_dupz(s.s, s.len), s.len);
}

static inline lstr_t t_lstr_dup(const lstr_t s)
{
    return T_LSTR_DUP(s.s, s.len);
}

static inline lstr_t mp_lstr_dup(mem_pool_t *mp, const lstr_t s)
{
    return LSTR_INIT_V(mp_dupz(mp, s.s, s.len), s.len);
}

#define CLSTR_INIT(s_, len_)  { .s = (s_), .len = (len_) }
#define CLSTR_INIT_V(s, len)  (clstr_t)CLSTR_INIT(s, len)
#define CLSTR_IMMED(str)      CLSTR_INIT(str, sizeof(str) - 1)
#define CLSTR_IMMED_V(str)    CLSTR_INIT_V(str, sizeof(str) - 1)
#define CLSTR_STR_V(str)      ({ const char *__s = (str); CLSTR_INIT_V(__s, strlen(__s)); })
#define CLSTR_NULL            CLSTR_INIT(NULL, 0)
#define CLSTR_NULL_V          CLSTR_INIT_V(NULL, 0)
#define CLSTR_EMPTY           CLSTR_INIT("", 0)
#define CLSTR_EMPTY_V         CLSTR_INIT_V("", 0)
#define CLSTR_SB(sb)          CLSTR_INIT((sb)->data, (sb)->len)
#define CLSTR_SB_V(sb)        CLSTR_INIT_V((sb)->data, (sb)->len)

#define T_CLSTR_DUP(str, len) ({ int __len = (len); CLSTR_INIT_V(t_dupz(str, __len), __len); })
#define T_CLSTR_DUP2(str)     ({ const char *__s = (str); T_CLSTR_DUP(__s, strlen(__s)); })

static inline clstr_t t_clstr_dup(const clstr_t s)
{
    return T_CLSTR_DUP(s.s, s.len);
}

static inline bool lstr_equal(const lstr_t *s1, const lstr_t *s2)
{
    return s1->len == s2->len && memcmp(s1->s, s2->s, s1->len) == 0;
}

static inline bool clstr_equal(const clstr_t *s1, const clstr_t *s2)
{
    return s1->len == s2->len && memcmp(s1->s, s2->s, s1->len) == 0;
}

#endif
