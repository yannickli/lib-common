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

#include <lib-common/parseopt.h>
#include <lib-common/unix.h>
#include <lib-common/iopc/iopc.h>

extern const char libcommon_git_revision[];

static struct {
    bool help, version, git_revision, wall;
    char *incpath;
    const char *lang;
    const char *outpath;
    const char *json_outpath;
    const char *c_outpath;
    const char *swift_outpath;
    const char *depends;
    const char *class_id_range;
} opts;

typeof(iopc_g) iopc_g = {
    .logger       = LOGGER_INIT_INHERITS(NULL, "iopc"),
    .class_id_min = 0,
    .class_id_max = 0xFFFF,
};
#define _G  iopc_g

static popt_t options[] = {
    OPT_FLAG('h', "help",         &opts.help,         "show this help"),
    OPT_FLAG('V', "version",      &opts.version,      "show version"),
    OPT_FLAG('G', "git-revision", &opts.git_revision, "show git revision"),

    OPT_GROUP(""),
    OPT_STR('I',  "include-path", &opts.incpath,  "include path"),
    OPT_STR('o',  "output-path",  &opts.outpath,  "base of the compiled hierarchy"),
    OPT_STR('d',  "depends",      &opts.depends,  "dump Makefile depends"),
    OPT_STR('l',  "language",     &opts.lang,     "output language (C)"),
    OPT_STR(0,  "class-id-range", &opts.class_id_range,
            "authorized class id range (min-max, included)"),
    OPT_FLAG(0,   "Wextra",       &_G.print_info,  "add extra warnings"),
    OPT_FLAG(0,  "check-snmp-table-has-index", &_G.check_snmp_table_has_index,
             "check each snmpTbl has an @snmpIndex"),
    OPT_FLAG('2', "features-v2",  &_G.v2,          "use iopc v2 features"),
    OPT_FLAG('3', "features-v3",  &_G.v3,          "use iopc v3 features"),
    OPT_FLAG('4', "features-v4",  &_G.v4,          "use iopc v4 features"),

    /* List of changes in v5:
     * - handling of \example doxygen tag (7e8bec96f68).
     * - trigger errors (and not warnings) when "avoid keywords" are
     *   used (ad5bf3fda).
     * - support @private attribute on classes (fa2055549).
     * - remove trailing '.' in briefs (54f5dc6b).
     * - deprecate 'import' feature (b8b9c529d).
     */
    OPT_FLAG('5', "features-v5",  &_G.v5,
             "use iopc v5 features (in progress)"),

    /* List of pending changes in v6:
     * - deprecate '%C{' features (b8b9c529d).
     * - unions use enum for iop_tag instead of int (#50352 / I87e09b3aef349)
     * - unions cannot be empty (#50352 / I5c9bc521e3ce)
     * - export nullability attributes on pointed types (Idfab7cb64ae16)
     * - export symbols (Ic430d3e80e12f3)
     * - minimal imports (Iad8aaaafb)
     */
    OPT_FLAG('6', "features-v6",  &_G.v6,
             "use iopc v6 features (in progress)"),

    OPT_GROUP("C backend options"),
    OPT_FLAG(0,   "c-resolve-includes", &iopc_do_c_g.resolve_includes,
             "try to generate relative includes"),
    OPT_FLAG(0,   "c-export-nullability", &iopc_do_c_g.export_nullability,
             "add nullability attributes to exported C types"),
    OPT_FLAG(0,   "c-unions-use-enums", &iopc_do_c_g.unions_use_enums,
             "use an enum for selected field in unions instead of an int"),
    OPT_FLAG(0,   "c-export-symbols", &iopc_do_c_g.export_symbols,
             "publicly export C symbols"),
    OPT_FLAG(0,   "c-minimal-includes", &iopc_do_c_g.minimal_includes,
             "include core.h/iop-internals.h instead of iop.h"),
    OPT_STR(0,    "c-output-path", &opts.c_outpath,
            "base of the compiled hierarchy for C files"),

    OPT_GROUP("JSON backend options"),
    OPT_STR(0,    "json-output-path", &opts.json_outpath,
            "base of the compiled hierarchy for JSON files"),

    OPT_GROUP("Swift backend options"),
    OPT_STR(0,    "swift-output-path", &opts.swift_outpath,
            "base of the compiled hierarchy for swift files"),
    OPT_STR(0,    "swift-import-modules", &iopc_do_swift_g.imported_modules,
            "comma-separated list of modules to import in swift files"),
    OPT_END(),
};

__attr_printf__(2, 0) static void
iopc_log_handler(const log_ctx_t *ctx, const char *fmt, va_list va)
{
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
}

static void parse_incpath(qv_t(cstr) *ipath, char *spec)
{
    if (!spec)
        return;
    for (;;) {
        if (!*spec) {
            qv_append(ipath, ".");
            return;
        }
        if (*spec == ':') {
            qv_append(ipath, ".");
        } else {
            qv_append(ipath, spec);
            spec = strchr(spec, ':');
            if (!spec)
                break;
        }
        *spec++ = '\0';
    }
    for (int i = ipath->len - 1; i >= 0; --i) {
        struct stat st;

        if (stat(ipath->tab[i], &st) || !S_ISDIR(st.st_mode)) {
            qv_remove(ipath, i);
        }
    }
}

static int parse_class_id_range(const char *class_id_range)
{
    pstream_t ps = ps_initstr(class_id_range);

    if (!ps_len(&ps))
        return 0;

    /* Format: "min-max" */
    iopc_g.class_id_min = ps_geti(&ps);
    THROW_ERR_IF(iopc_g.class_id_min < 0 || iopc_g.class_id_min > 0xFFFF);

    RETHROW(ps_skipc(&ps, '-'));

    iopc_g.class_id_max = ps_geti(&ps);
    THROW_ERR_IF(iopc_g.class_id_max < iopc_g.class_id_min
              || iopc_g.class_id_max > 0xFFFF);

    return 0;
}

static char const * const only_stdin[] = { "-", NULL };

struct doit {
    int (*cb)(iopc_pkg_t *, const char *, sb_t *);
    const char *outpath;
};
qvector_t(doit, struct doit);

static int build_doit_table(qv_t(doit) *doits)
{
    qv_t(lstr) langs;
    ctype_desc_t sep;
    bool has_swift = false;
    bool has_c = false;

    /* default languages */
    if (!opts.lang) {
        opts.lang = "c";
    }
    qv_inita(&langs, 2);
    ctype_desc_build(&sep, ",");
    ps_split(ps_initstr(opts.lang), &sep, 0, &langs);

    tab_for_each_entry(lang, &langs) {
        struct doit doit;

        if (lstr_ascii_iequal(lang, LSTR("c"))) {
            has_c = true;
            doit = (struct doit){
                .cb = &iopc_do_c,
                .outpath = opts.c_outpath
            };
        } else
        if (lstr_ascii_iequal(lang, LSTR("json"))) {
            doit = (struct doit){
                .cb = &iopc_do_json,
                .outpath = opts.json_outpath
            };
        } else
        if (lstr_ascii_iequal(lang, LSTR("swift"))) {
            has_swift = true;
            iopc_do_c_g.include_swift_support = true;
            doit = (struct doit){
                .cb = &iopc_do_swift,
                .outpath = opts.swift_outpath
            };
        } else {
            print_error("unsupported language `%*pM`", LSTR_FMT_ARG(lang));
            goto error;
        }

        if (doit.outpath) {
            if (mkdir_p(doit.outpath, 0777) < 0) {
                print_error("cannot create output directory `%s`: %m",
                            doit.outpath);
                goto error;
            }
        }
        qv_append(doits, doit);
    }

    if (has_swift && !has_c) {
        print_error("Swift backend requires C backend");
        goto error;
    }

    qv_wipe(&langs);
    return 0;

  error:
    qv_wipe(&langs);
    return -1;
}

int main(int argc, char **argv)
{
    const char *arg0 = NEXTARG(argc, argv);
    qv_t(cstr) incpath;
    qv_t(doit) doits;
    iopc_pkg_t *pkg;
    SB_8k(deps);

    setlinebuf(stderr);
    argc = parseopt(argc, argv, options, 0);
    if (argc < 0 || opts.help) {
        makeusage(!opts.help, arg0, "<iop file>", NULL, options);
    }
    if (opts.version) {
        printf("%d.%d.%d\n", IOPC_MAJOR, IOPC_MINOR, IOPC_PATCH);
        return 0;
    }
    if (opts.git_revision) {
        printf("%s\n", libcommon_git_revision);
        return 0;
    }

    MODULE_REQUIRE(iopc);

    qv_init(&incpath);
    qv_init(&doits);

    opts.c_outpath    = opts.c_outpath ?: opts.outpath;
    opts.json_outpath = opts.json_outpath ?: opts.outpath;

    _G.v5 |= _G.v6;
    _G.v4 |= _G.v5;
    _G.v3 |= _G.v4;
    _G.v2 |= _G.v3;

    iopc_do_c_g.export_nullability |= _G.v6;
    iopc_do_c_g.export_nullability |= iopc_do_c_g.include_swift_support;
    iopc_do_c_g.export_symbols |= _G.v6;
    iopc_do_c_g.minimal_includes |= _G.v6;

    _G.prefix_dir     = getcwd(NULL, 0);
    _G.display_prefix = true;

    log_set_handler(&iopc_log_handler);

    if (opts.c_outpath && iopc_do_c_g.resolve_includes) {
        print_error("outdir and --c-resolve-includes are incompatible");
        goto error;
    }

    if (build_doit_table(&doits) < 0) {
        goto error;
    }

    parse_incpath(&incpath, opts.incpath);

    if (opts.class_id_range && parse_class_id_range(opts.class_id_range) < 0)
    {
        print_error("invalid class-id-range `%s`", opts.class_id_range);
        goto error;
    }

    if (argc == 0) {
        argc = 1;
        argv = (char **)only_stdin;
    }
    for (int i = 0; i < argc; i++) {
        iopc_parser_typer_initialize();

        if (!(pkg = iopc_parse_file(&incpath, NULL, argv[i], NULL, true))) {
            iopc_parser_typer_shutdown();
            goto error;
        }
        if (iopc_resolve(pkg) < 0) {
            iopc_parser_typer_shutdown();
            goto error;
        }
        if (iopc_resolve_second_pass(pkg) < 0) {
            iopc_parser_typer_shutdown();
            goto error;
        }

        iopc_types_fold(pkg);

        tab_for_each_ptr(doit, &doits) {
            if ((*doit->cb)(pkg, doit->outpath, &deps) < 0) {
                iopc_parser_typer_shutdown();
                goto error;
            }
        }

        iopc_parser_typer_shutdown();
    }

    if (opts.depends) {
        char dir[PATH_MAX];

        path_dirname(dir, sizeof(dir), opts.depends);
        if (mkdir_p(dir, 0755) < 0) {
            print_error("cannot create directory `%s`: %m", dir);
            goto error;
        }
        if (sb_write_file(&deps, opts.depends) < 0) {
            print_error("cannot write file `%s`: %m", opts.depends);
            goto error;
        }
    }

    MODULE_RELEASE(iopc);
    qv_wipe(&incpath);
    qv_wipe(&doits);
    p_delete((char **)&_G.prefix_dir);
    return 0;

  error:
    MODULE_RELEASE(iopc);
    qv_wipe(&incpath);
    qv_wipe(&doits);
    p_delete((char **)&_G.prefix_dir);
    return -1;
}
