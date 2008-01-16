/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "conf.h"

struct parse_state {
    flag_t seen_section : 1;
    flag_t was_section  : 1;
};

static int parse_hook(void *_ps, cfg_parse_evt evt,
                      const char *v, int vlen, void *ctx)
{
    struct parse_state *ps = _ps;

    switch (evt) {
      case CFG_PARSE_SECTION:
        if (ps->seen_section)
            putchar('\n');
        printf("[%s", v);
        ps->seen_section = ps->was_section = true;
        return 0;

      case CFG_PARSE_SECTION_ID:
        ps->was_section = false;
        printf(" \"%s\"]\n", v);
        return 0;

      case CFG_PARSE_KEY:
        if (ps->was_section) {
            printf("]\n");
            ps->was_section = false;
        }
        printf("%s = ", v);
        return 0;

      case CFG_PARSE_VALUE:
        printf("%s\n", v ?: "");
        return 0;

      case CFG_PARSE_EOF:
        if (ps->was_section) {
            printf("]\n");
            ps->was_section = false;
        }
        return 0;

      case CFG_PARSE_ERROR:
        fprintf(stderr, "%s\n", v);
        return 0;

      case CFG_PARSE_KEY_ARRAY:
        abort();
    }
    return -1;
}

int main(int argc, const char **argv)
{
    for (argc--, argv++; argc > 0; argc--, argv++) {
        struct parse_state ps = {
            .seen_section = false,
        };
        cfg_parse(*argv, &parse_hook, &ps, CFG_PARSE_OLD_NAMESPACES);
    }
    return 0;
}
