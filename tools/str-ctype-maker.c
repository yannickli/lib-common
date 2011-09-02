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

#include <lib-common/str.h>
#include <lib-common/parseopt.h>

int main(int argc, char **argv)
{
    SB_1k(buf);

    if (argc <= 2) {
        puts("usage: str-ctype-maker varname < chars | range >...");
        puts("");
        puts("  chars: s:<c-quoted list of chars>");
        puts("  range: r:code1[-code2] (code can be decimal, hexa or octal)");
        return 1;
    }

    sb_init(&buf);
    for (int i = 2; i < argc; i++) {
        const char *s = argv[i];

        if (strstart(s, "s:", &s)) {
            sb_adds_unquoted(&buf, argv[2] + 2);
            continue;
        }

        if (strstart(s, "r:", &s)) {
            long l, r;

            errno = 0;
            l = strtol(s, &s, 0);
            if (errno)
                goto range_error;

            if (*s == '-') {
                s++;
                r = strtol(s, &s, 0);
                if (errno)
                    goto range_error;
            } else {
                r = l;
            }
            if (*s)
                goto range_error;
            if ((uint8_t)r != r || (uint8_t)l != l || l > r)
                goto range_error;
            for (int c = l; c <= r; c++)
                sb_addc(&buf, c);
            continue;

          range_error:
            e_fatal("invalid range: %s", argv[i]);
        }

        e_fatal("invalid argument: %s", argv[i]);
    }

    {
        ctype_desc_t d;

        ctype_desc_build2(&d, buf.data, buf.len);

        printf("ctype_desc_t const %s = { {\n", argv[1]);
        printf("    0x%08x, 0x%08x, 0x%08x, 0x%08x,\n",
               d.tab[0], d.tab[1], d.tab[2], d.tab[3]);
        printf("    0x%08x, 0x%08x, 0x%08x, 0x%08x,\n",
               d.tab[4], d.tab[5], d.tab[6], d.tab[7]);
        puts("} };");
    }

    sb_wipe(&buf);
    return 0;
}
