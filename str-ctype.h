/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2017 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_CORE_H) || defined(IS_LIB_COMMON_STR_CTYPE_H)
#  error "you must include core.h instead"
#else
#define IS_LIB_COMMON_STR_CTYPE_H

/* @typedef ctype_desc_t
 * @brief is an array of 256 bits which represents in
 * common use the 256 8-bitted characters. Then one can
 * tests and sets the presence of every characters by modifying
 * a single bit
 */
typedef struct __swift_name__("CType") ctype_desc_t {
    uint32_t tab[256 / 32];
} ctype_desc_t;

__swift_name__("CType.isAlnum")
extern ctype_desc_t const ctype_isalnum;
__swift_name__("CType.isAlpha")
extern ctype_desc_t const ctype_isalpha;
__swift_name__("CType.isLower")
extern ctype_desc_t const ctype_islower;
__swift_name__("CType.isUpper")
extern ctype_desc_t const ctype_isupper;
__swift_name__("CType.isDigit")
extern ctype_desc_t const ctype_isdigit;
__swift_name__("CType.isSpace")
extern ctype_desc_t const ctype_isspace;
__swift_name__("CType.isHexDigit")
extern ctype_desc_t const ctype_ishexdigit;
__swift_name__("CType.isBinDigit")
extern ctype_desc_t const ctype_isbindigit;
__swift_name__("CType.isWordPart")
extern ctype_desc_t const ctype_iswordpart;
__swift_name__("CType.isCVar")
extern ctype_desc_t const ctype_iscvar;

/* @func ctype_desc_reset
 * @param[in] d
 */
__swift_name__("CType.reset(self:)")
static inline void ctype_desc_reset(ctype_desc_t * nonnull d)
{
    p_clear(d, 1);
}

/* @func ctype_desc_build
 * @param[in] d
 * @param[in] toks string of characters containing a token at every
 *                 characters.
 */
__swift_name__("CType.buildFromCString(self:_:)")
static inline void ctype_desc_build(ctype_desc_t * nonnull d,
                                    const char * nonnull toks)
{
    ctype_desc_reset(d);
    while (*toks) {
        SET_BIT(d->tab, (unsigned char)*toks);
        toks++;
    }
}

__swift_name__("CType.buildFromBuffer(self:_:count:)")
static inline void
ctype_desc_build2(ctype_desc_t * nonnull d, const char * nonnull toks,
                  int len)
{
    ctype_desc_reset(d);
    for (int i = 0; i < len; i++) {
        SET_BIT(d->tab, (unsigned char)toks[i]);
    }
}

/* TODO adding more functions to manipulate the ctype_desc_t
 *      structure(unset, toggle, ...).
 */

/* @func ctype_desc_contains
 * @param[in] d
 * @param[in] b 
 * @return the function returns true in the case b is present in d
 * @brief This function checks if a byte is set on in a ctype_desc
 *        structure
 */
__swift_name__("CType.contains(self:_:)")
static inline bool ctype_desc_contains(const ctype_desc_t * nonnull d, byte b)
{
    return TST_BIT(d->tab, b);
}

/* @func ctype_desc_combine
 * param[in] d1
 * param[in] d2
 * param[inout] dout
 * TODO binary operation on memory instead of a stupid bit per bit operating
 */
__swift_name__("CType.combine(self:_:_:)")
static inline void
ctype_desc_combine(ctype_desc_t * nonnull dst,
                   const ctype_desc_t * nonnull d1,
                   const ctype_desc_t * nonnull d2)
{
    for (int i = 0; i < countof(d1->tab); i++) {
        dst->tab[i] = d1->tab[i] | d2->tab[i];
    }
}

/* @func ctype_desc_invert
 * param[inout] d
 */
__swift_name__("CType.invert(self:)")
static inline void ctype_desc_invert(ctype_desc_t * nonnull d)
{
    for (int i = 0 ; i < countof(d->tab) ; i++)
        d->tab[i] = ~(d->tab[i]);
}

#endif
