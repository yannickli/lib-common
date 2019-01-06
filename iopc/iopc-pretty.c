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

#include "iopc.h"

const char *t_pretty_token(iopc_tok_type_t token)
{
    if (isprint(token)) {
        return t_fmt("`%c`", token);
    }
    switch (token) {
      case ITOK_EOF:           return "end of file";
      case ITOK_IDENT:         return "identifier";
      case ITOK_LSHIFT:        return "`<<`";
      case ITOK_RSHIFT:        return "`>>`";
      case ITOK_EXP:           return "`**`";
      case ITOK_INTEGER:       return "integer";
      case ITOK_DOUBLE:        return "double";
      case ITOK_BOOL:          return "boolean";
      case ITOK_STRING:        return "string";
      case ITOK_COMMENT:       return "comment";
      case ITOK_DOX_COMMENT:   return "doxygen comment";
      case ITOK_VERBATIM_C:    return "verbatim c";
      case ITOK_ATTR:          return "attribute";
      case ITOK_GEN_ATTR_NAME: return "generic attribute name (namespace:id)";
      default:                 return "unknown token";
    }
}

const char *pretty_path_dot(iopc_path_t *path)
{
    if (!path->pretty_dot) {
        sb_t b;
        sb_init(&b);

        for (int i = 0; i < path->bits.len; i++)
            sb_addf(&b, "%s.", path->bits.tab[i]);
        sb_shrink(&b, 1);
        path->pretty_dot = sb_detach(&b, NULL);
    }
    return path->pretty_dot;
}

const char *pretty_path(iopc_path_t *path)
{
    if (!path->pretty_slash) {
        sb_t b;
        sb_init(&b);

        for (int i = 0; i < path->bits.len; i++)
            sb_addf(&b, "%s/", path->bits.tab[i]);
        b.data[b.len - 1] = '.';
        sb_adds(&b, "iop");
        path->pretty_slash = sb_detach(&b, NULL);
    }
    return path->pretty_slash;
}
