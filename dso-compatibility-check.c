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

#include "iop.h"
#include "parseopt.h"
#include <sysexits.h>

static struct compat_g {
    bool help;
    const char *new_dso;
    const char *old_dso;
    const char *mode;
} compat_g = {
#define _G compat_g
    .help = 0,
};

static iop_dso_t *open_dso(const char *dso_path)
{
    SB_1k(err);
    iop_dso_t *dso;

    dso = iop_dso_open(dso_path, LM_ID_BASE, &err);
    if (dso == NULL) {
        e_fatal("unable to load `%s` (%*pM)\n", dso_path,
                SB_FMT_ARG(&err));
    }
    return dso;
}

static const char *usage[] = {
    "print out on stderr if a newer dso is compatible with an old one by ",
    "checking all packages.",
    "",
    NULL
};

static struct popt_t popt[] = {
    OPT_GROUP("Options:"),
    OPT_FLAG('h', "help", &_G.help, "show this help"),
    OPT_STR('n', "new_dso", &_G.new_dso, "the absolute path to the new "
            "dso file"),
    OPT_STR('o', "old_dso", &_G.old_dso, "the absolute path to the old "
            "dso file"),
    OPT_STR('m', "mode", &_G.mode, "check dso compatibility by checking: "
            "JSON, BINARY, ALL (default)"),
    OPT_END(),
};

int main(int argc, char *argv[])
{
    iop_dso_t *dso;
    iop_dso_t *dso_old;
    iop_compat_ctx_t *ctx;
    unsigned flags;
    SB_1k(err);
    lstr_t mode;
    const char *arg0 = NEXTARG(argc, argv);

    if (!argc) {
        makeusage(EX_USAGE, arg0, "", usage, popt);
    }

    argc = parseopt(argc, argv, popt, 0);

    if (argc != 0 || _G.help || !_G.old_dso || !_G.new_dso) {
        makeusage(EX_USAGE, arg0, "", usage, popt);
    }

    mode = _G.mode ? LSTR(_G.mode) : LSTR_EMPTY_V;

    if (_G.mode) {
        if (lstr_ascii_iequal(mode, LSTR("JSON"))) {
            flags = IOP_COMPAT_JSON;
        }
        else
        if (lstr_ascii_iequal(mode, LSTR("BINARY"))) {
            flags = IOP_COMPAT_BIN;
        }
        else
        if (lstr_ascii_iequal(mode, LSTR("ALL"))) {
            flags = IOP_COMPAT_ALL;
        }
        else {
            e_fatal("unkown mode `%*pM`", LSTR_FMT_ARG(mode));
        }
    } else {
        flags = IOP_COMPAT_ALL;
    }

    dso = open_dso(_G.new_dso);
    dso_old = open_dso(_G.old_dso);
    ctx = iop_compat_ctx_new();

    qm_for_each_pos(iop_pkg, pos, &dso_old->pkg_h) {
        const iop_pkg_t *pkg_old = dso_old->pkg_h.values[pos];
        const iop_pkg_t *pkg;

        pkg = qm_get_def(iop_pkg, &dso->pkg_h, &pkg_old->name, NULL);

        if (!pkg) {
            fprintf(stderr, "package `%*pM` is not in the new dso\n",
                    LSTR_FMT_ARG(pkg_old->name));
            continue;
        }

        if (iop_pkg_check_backward_compat_ctx(pkg_old, pkg, ctx,
                                              flags, &err) < 0)
        {
            fprintf(stderr, "%*pM\n", SB_FMT_ARG(&err));
            sb_reset(&err);
        }
    }

    iop_compat_ctx_delete(&ctx);
    iop_dso_close(&dso);
    iop_dso_close(&dso_old);

    return 0;
}
