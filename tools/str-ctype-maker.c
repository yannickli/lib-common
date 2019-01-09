/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
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

int main(int argc, char **argv)
{
    sb_t buf;
    ctype_desc_t d;
    int i;

    if (argc != 3) {
        e_trace(0, "Usage: str-ctype-maker varname <tokens>");
        return 1;
    }

    sb_init(&buf);
    sb_adds_unquoted(&buf, argv[2]);
    e_trace(0, "%d characters read", buf.len);
    e_trace(0, "here are the data found:\n'%s'", buf.data);

    ctype_desc_build2(&d, buf.data, buf.len);

    printf("\n/* ctype description for tokens \"%s\" */\n", argv[2]);
    printf("ctype_desc_t const %s = {\n    {\n", argv[1]);
    for ( i = 0 ; i < countof(d.tab) ; i++)
        printf("        0x%08x,\n", d.tab[i]);
    puts("    }\n};\n\n");

    return 0;
}
