/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#if !defined(IS_LIB_COMMON_IOP_H) || defined(IS_LIB_COMMON_IOP_XML_H)
#  error "you must include <lib-common/iop.h> instead"
#else
#define IS_LIB_COMMON_IOP_XML_H

/*----- IOP to SOAP interfaces -----*/
/* XXX: Assumes xsi points to http://www.w3.org/2001/XMLSchema-instance
 *      and xsd to http://www.w3.org/2001/XMLSchema
 * XXX: Assumes that these options are given to the xml parser:
 *      XML_PARSE_NOENT | XML_PARSE_NOBLANKS | XML_PARSE_NONET
 *      | XML_PARSE_NOCDATA
 */
void iop_xpack(sb_t *sb, const iop_struct_t *, const void *v,
               bool verbose, bool with_enums);

/* flags is a bitfield of iop_unpack_flags */
int iop_xunpack_flags(void *xp, mem_pool_t *mp, const iop_struct_t *, void *v,
                      int flags);
int iop_xunpack(void *xp, mem_pool_t *mp, const iop_struct_t *, void *v);

qh_k32_t(xwsdl_impl);
void iop_xwsdl(sb_t *sb, const iop_mod_t *mod, qh_t(xwsdl_impl) *impl,
               const char *ns, const char *addr, bool with_auth,
               bool with_enums);
#define xwsdl_register(h, _mod, _if, _rpc) \
    qh_add(xwsdl_impl, h, IOP_RPC_CMD(_mod, _if, _rpc))

#endif
