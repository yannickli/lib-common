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

#include <dlfcn.h>
#include <sys/wait.h>

#include <lib-common/unix.h>
#include <lib-common/farch.h>
#include <lib-common/core.h>

#include "iopc.h"
#include "iopc.fc.c"

typeof(iopc_g) iopc_g = {
    .logger       = LOGGER_INIT_INHERITS(NULL, "iopc"),
    .class_id_min = 0,
    .class_id_max = 0xFFFF,
};

static struct {
    logger_t logger;
} iopc_so_g = {
#define _G  iopc_so_g
    .logger = LOGGER_INIT_INHERITS(&iopc_g.logger, "dso"),
};

static int do_call(char * const argv[])
{
    pid_t pid;

    pid = fork();
    if (pid < 0) {
        logger_fatal(&_G.logger, "unable to fork(): %m");
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        logger_fatal(&_G.logger, "unable to run %s: %m", argv[0]);
    }

    for (;;) {
        int status;

        if (waitpid(pid, &status, 0) < 0) {
            logger_fatal(&_G.logger, "waitpid: %m");
        }
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status) ? -1 : 0;
        }
        if (WIFSIGNALED(status)) {
            logger_fatal(&_G.logger, "%s killed with signal %s", argv[0],
                         sys_siglist[WTERMSIG(status)]);
        }
    }

    return 0;
}

static int do_compile(const qv_t(str) *in, const char *out)
{
    t_scope;
    qv_t(cstr) args;

    t_qv_init(cstr, &args, 20);

    qv_append(cstr, &args, "cc");

    qv_append(cstr, &args, "-std=gnu99");
    qv_append(cstr, &args, "-shared");
    qv_append(cstr, &args, "-fPIC");

    qv_append(cstr, &args, "-Wall");
    qv_append(cstr, &args, "-Werror");
    qv_append(cstr, &args, "-Wextra");
    qv_append(cstr, &args, "-Wno-unused-parameter");

#ifdef NDEBUG
    qv_append(cstr, &args, "-s");                       /* strip DSO        */
    qv_append(cstr, &args, "-O3");
#else
    qv_append(cstr, &args, "-O0");
    /* XXX valgrind does not support loading dso built with -g3, it fails with
     * "Warning: DWARF2 reader: Badly formed extended line op encountered"
     */
    if (RUNNING_ON_VALGRIND) {
        qv_append(cstr, &args, "-g");
    } else {
        qv_append(cstr, &args, "-g3");
    }
#endif
    qv_append(cstr, &args, "-fno-strict-aliasing");

    qv_append(cstr, &args, "-o");
    qv_append(cstr, &args, out);                        /* DSO output       */
    qv_for_each_entry(str, s, in) {
        qv_append(cstr, &args, s);
    }
    qv_append(cstr, &args, NULL);

    return do_call((char * const *)args.tab);
}

static void
iopc_build(const qm_t(env) *env, const char *iopfile, const char *iopdata,
           const char *outdir, bool is_main_pkg, lstr_t *pkgname,
           lstr_t *pkgpath)
{
    SB_1k(sb);
    iopc_pkg_t *pkg;
    const farch_entry_t *farch;

    farch = farch_get(iopc_farch, "../iop-compat.h");
    sb_adds(&sb, farch->data);
    farch = farch_get(iopc_farch, "../iop-internals.h");
    sb_adds(&sb, farch->data);

    iopc_g.v2 = true;
    iopc_g.v3 = true;
    iopc_g.v4 = true;
    iopc_do_c_g.resolve_includes = false;
    iopc_do_c_g.no_const = true;
    iopc_do_c_g.iop_compat_header = sb.data;

    iopc_parser_initialize();
    pkg = iopc_parse_file(NULL, env, iopfile, iopdata, is_main_pkg);
    iopc_resolve(pkg);
    iopc_resolve_second_pass(pkg);
    iopc_types_fold(pkg);
    iopc_do_c(pkg, outdir, NULL);
    if (is_main_pkg) {
        iopc_do_json(pkg, outdir, NULL);
    }

    if (pkgname) {
        *pkgname = lstr_dups(pretty_path_dot(pkg->name), -1);
    }
    if (pkgpath) {
        *pkgpath = lstr_dups(pretty_path(pkg->name), -1);
    }

    iopc_parser_shutdown();
}

void iopc_dso_set_class_id_range(uint16_t class_id_min, uint16_t class_id_max)
{
    iopc_g.class_id_min = class_id_min;
    iopc_g.class_id_max = class_id_max;
}

int iopc_dso_build(const char *iopfile, const qm_t(env) *env,
                   const char *outdir)
{
    SB_1k(sb);
    lstr_t pkgname, pkgpath;
    char so_path[PATH_MAX], path[PATH_MAX], tmppath[PATH_MAX];
    char json_path[PATH_MAX];
    qv_t(str) sources;
    int ret = 0;
    const char *filepart = path_filepart(iopfile);
    struct stat iop_st;

    qv_init(str, &sources);

    path_extend(so_path, outdir, "%s.so", filepart);

    if (stat(iopfile, &iop_st) < 0) {
        return logger_error(&_G.logger, "unable to stat IOP file %s: %m",
                            iopfile);
    }

    path_extend(tmppath, outdir, "%s.%d.XXXXXX", filepart, getpid());
    if (!mkdtemp(tmppath)) {
        return logger_error(&_G.logger,
                            "failed to create temporary directory %s: %m",
                            tmppath);
    }

    iopc_build(env, iopfile, NULL, tmppath, true, &pkgname, &pkgpath);

    /* move json to outdir */
    path_extend(json_path, outdir, "%*pM.json", LSTR_FMT_ARG(pkgpath));
    path_extend(path, tmppath, "%*pM.json",  LSTR_FMT_ARG(pkgpath));
    if (rename(path, json_path) < 0) {
        return logger_error(&_G.logger, "failed to create json file %s: %m",
                            json_path);
    }

    path_extend(path, tmppath, "%*pM.c",  LSTR_FMT_ARG(pkgpath));
    qv_append(str, &sources, p_strdup(path));

    sb_addf(&sb, "#include \"%*pM.h\"\n", LSTR_FMT_ARG(pkgpath));
    sb_adds(&sb, "IOP_EXPORT_PACKAGES_COMMON;\n");
    sb_adds(&sb, "IOP_USE_EXTERNAL_PACKAGES;\n");
    sb_addf(&sb, "IOP_EXPORT_PACKAGES(&%*pM__pkg);\n", LSTR_FMT_ARG(pkgname));

    path_extend(path, tmppath, "%*pM-iop-plugin.c", LSTR_FMT_ARG(pkgname));
    sb_write_file(&sb, path);
    qv_append(str, &sources, p_strdup(path));

    qm_for_each_pos(env, pos, env) {
        lstr_t deppath;
        const char *depfile = env->keys[pos];
        const char *depdata = env->values[pos];

        iopc_build(env, depfile, depdata, tmppath, false, NULL, &deppath);
        path_extend(path, tmppath, "%*pM.c", LSTR_FMT_ARG(deppath));
        qv_append(str, &sources, p_strdup(path));
        lstr_wipe(&deppath);
    }

    if (do_compile(&sources, so_path) < 0) {
        ret = logger_error(&_G.logger, "failed to build %s", so_path);
        goto end;
    }
    logger_info(&_G.logger, "iop plugin %s successfully built from %s",
                so_path, iopfile);

  end:
    qv_deep_wipe(str, &sources, p_delete);
    lstr_wipe(&pkgname);
    lstr_wipe(&pkgpath);
    rmdir_r(tmppath, false);
    return ret;
}
