/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2018 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include <glob.h>
#include <lib-common/parseopt.h>
#include <lib-common/qlzo.h>
#include <lib-common/unix.h>
#include <lib-common/farch.h>

static struct opts_g {
    const char *out;
    const char *target;
    const char *deps;
    int help;
    int verbose;
    int compress_lzo;
} opts_g;

static popt_t popt[] = {
    OPT_FLAG('h', "help",         &opts_g.help,    "show help"),
    OPT_FLAG('v', "verbose",      &opts_g.verbose, "be verbose"),
    OPT_STR('d',  "deps",         &opts_g.deps,    "build depends file"),
    OPT_STR('o',  "output",       &opts_g.out,
            "output to this file, default: stdout"),
    OPT_STR('T',  "target",       &opts_g.target,
            "add that to the dep target"),
    OPT_FLAG('c', "compress-lzo", &opts_g.compress_lzo,
             "compress files using LZO algorithm"),
    OPT_END(),
};

static void panic_purge(void)
{
    if (opts_g.out) {
        unlink(opts_g.out);
    }
    exit(EXIT_FAILURE);
}

#define TRACE(fmt, ...) \
    do { if (opts_g.verbose) e_info("farchc: "fmt, ##__VA_ARGS__); } while (0)

#define DIE_IF(tst, fmt, ...) \
    do { if (tst) { e_error(fmt, ##__VA_ARGS__); panic_purge(); } } while (0)

static void copy_dir(char *buf, int sz, const char *s)
{
    /* OG: need a generic makepath utility function */
    int len = snprintf(buf, sz, "%s/", s);
    assert (len < sz);
    while (len > 0 && buf[len - 1] == '/')
        len--;
    buf[len + 1] = '\0';
}

static void put_as_str(const char *data, int len, FILE *out)
{
    for (int i = 0; i < len; i++) {
        fprintf(out, "\\x%x", data[i]);
    }
}

static void put_chunk(const char *data, int len, int xor_data_key, FILE *out)
{
    fprintf(out, "{\n"
            "    .chunk_size = %d,\n"
            "    .xor_data_key = %d,\n"
            "    .chunk = \"", len, xor_data_key);
    put_as_str(data, len, out);
    fprintf(out, "\",\n"
            "},\n");
}

static int dump_and_obfuscate(const char *data, int len, FILE *out)
{
    int nb_chunk = 0;

    while (len > 0) {
        char obfuscated_chunk[FARCH_MAX_SYMBOL_SIZE];
        int xor_data_key;
        int chunk_size;

        chunk_size = rand_range(FARCH_MAX_SYMBOL_SIZE / 2,
                                FARCH_MAX_SYMBOL_SIZE);
        chunk_size = MIN(len, chunk_size);
        farch_obfuscate(data, chunk_size, &xor_data_key, obfuscated_chunk);
        put_chunk(obfuscated_chunk, chunk_size, xor_data_key, out);
        data += chunk_size;
        len -= chunk_size;
        nb_chunk++;
    }

    assert (len == 0);

    return nb_chunk;
}

static void dump_file(const char *path, farch_entry_t *entry, FILE *out)
{
    lstr_t file;

    DIE_IF(lstr_init_from_file(&file, path, PROT_READ, MAP_SHARED) < 0,
           "unable to open `%s` for reading: %m", path);
    entry->size = file.len;

    if (opts_g.compress_lzo) {
        t_scope;
        byte lzo_buf[LZO_BUF_MEM_SIZE];
        char *cbuf;
        int clen;

        clen = lzo_cbuf_size(file.len);
        cbuf = t_new_raw(char, clen);
        clen = qlzo1x_compress(cbuf, clen, ps_initlstr(&file), lzo_buf);

        if (clen < file.len) {
            entry->nb_chunks = dump_and_obfuscate(cbuf, clen, out);
            entry->compressed_size = clen;
        } else {
            entry->nb_chunks = dump_and_obfuscate(file.s, file.len, out);
            entry->compressed_size = file.len;
        }
    } else {
        entry->nb_chunks = dump_and_obfuscate(file.s, file.len, out);
        entry->compressed_size = file.len;
    }

    lstr_wipe(&file);
}

qvector_t(farch_entry, farch_entry_t);

static void dump_entries(const char *archname,
                         const qv_t(farch_entry) *entries, FILE *out)
{
    int chunk = 0;

    tab_for_each_ptr(entry, entries) {
        char obfuscated_name[2 * PATH_MAX];
        int xor_name_key;
        int len = entry->name.len;

        farch_obfuscate(entry->name.s, len, &xor_name_key, obfuscated_name);

        fprintf(out, "/* {""{{ %*pM */\n", LSTR_FMT_ARG(entry->name));
        fprintf(out, "{\n"
                "    .name = LSTR_IMMED(\"");
        put_as_str(obfuscated_name, len, out);
        fprintf(out, "\"),\n"
                "    .data = &%s_data[%d],\n"
                "    .size = %d,\n"
                "    .compressed_size = %d,\n"
                "    .nb_chunks = %d,\n"
                "    .xor_name_key = %d,\n"
                "},\n"
                "/* }""}} */\n",
                archname, chunk, entry->size, entry->compressed_size,
                entry->nb_chunks, xor_name_key);
        chunk += entry->nb_chunks;
    }
}

static int do_work(const char *reldir, FILE *in, FILE *out, FILE *deps)
{
    t_scope;
    char prefix[PATH_MAX] = "", srcdir[PATH_MAX] = "";
    char name[PATH_MAX], buf[PATH_MAX], path[PATH_MAX];
    int srcdirlen = 0;
    sb_t dep;
    qv_t(farch_entry) entries;

    t_qv_init(&entries, 10);

    do {
        DIE_IF(!fgets(name, sizeof(name), in), "no variable name specified: %m");
        DIE_IF(name[strlen(name) - 1] != '\n', "line 1 is too long: %m");
    } while (*skipspaces(name) == '#' || *skipspaces(name) == '\0');
    strrtrim(name);
    TRACE("creating `%s`", name);

    fprintf(out, "/* This file is generated by farchc. */\n");
    fprintf(out, "\n");
    fprintf(out, "#include <lib-common/farch.h>\n");
    fprintf(out, "\n");
    fprintf(out, "static const farch_data_t %s_data[] = {\n", name);

    sb_inita(&dep, BUFSIZ);
    if (opts_g.target)
        sb_addf(&dep, "%s%s ", reldir, opts_g.target);
    if (opts_g.out)
        sb_addf(&dep, "%s ", opts_g.out);
    sb_addf(&dep, "%s: ", opts_g.deps);

    snprintf(srcdir, sizeof(srcdir), "%s", reldir);
    path_simplify(srcdir);
    path_join(srcdir, sizeof(srcdir), "/");
    srcdirlen = strlen(srcdir);

    for (int lineno = 2; fgets(buf, sizeof(buf), in); lineno++) {
        const char *s = skipspaces(buf);
        glob_t gbuf;

        DIE_IF(buf[strlen(buf) - 1] != '\n', "line %d is too long", lineno);
        strrtrim(buf);
        if (!*s)
            continue;
        if (*s == '#') {
            TRACE("%s", s);
            continue;
        }
        if (strstart(s, "CD", &s)) {
            if (isspace((unsigned char)*s)) {
                snprintf(srcdir, sizeof(srcdir), "%s/%s", reldir, skipspaces(s));
                path_simplify(srcdir);
                path_join(srcdir, sizeof(srcdir), "/");
                TRACE("changing directory to `%s`", srcdir);
                srcdirlen = strlen(srcdir);
                continue;
            } else if (!*s) {
                strcpy(srcdir, "./");
                srcdirlen = 2;
                TRACE("changing directory to `.`");
                continue;
            }
            s = skipspaces(buf);
        }
        if (strstart(s, "PREFIX", &s)) {
            if (isspace((unsigned char)*s)) {
                copy_dir(prefix, sizeof(prefix), skipspaces(s));
                TRACE("changing prefix to `%s`", prefix);
                continue;
            } else if (!*s) {
                *srcdir = '\0';
                TRACE("changing prefix to ``");
                continue;
            }
            s = skipspaces(buf);
        }

        snprintf(path, sizeof(path), "%s%s", srcdir, s);
        DIE_IF(glob(path, 0, NULL, &gbuf), "no file matching `%s`: %m", path);
        if (gbuf.gl_pathc >= 1 && !strequal(gbuf.gl_pathv[0], path)) {
            TRACE("globbing `%s` (%zd matches)", path + srcdirlen, gbuf.gl_pathc);
            if (deps)
                fprintf(deps, "%s$(wildcard %s)\n", dep.data, path);
        }
        for (size_t i = 0; i < gbuf.gl_pathc; i++) {
            farch_entry_t entry;
            const char *p = gbuf.gl_pathv[i];
            char *fullname = t_fmt("%s%s", prefix, p + srcdirlen);

            if (deps) {
                fprintf(deps, "%s%s\n", dep.data, p);
                fprintf(deps, "%s:\n", p);
            }

            TRACE("adding `%s` as `%s`", p, fullname);
            entry.name = LSTR(fullname);
            fprintf(out, "/* {""{{ %s */\n", fullname);
            dump_file(p, &entry, out);
            fprintf(out, "/* }""}} */\n");
            qv_append(&entries, entry);
        }
        globfree(&gbuf);
    }

    fprintf(out, "};\n\n"
            "static const farch_entry_t %s[] = {\n", name);
    dump_entries(name, &entries, out);

    sb_wipe(&dep);
    fprintf(out,
            "{   .name = LSTR_NULL },\n"
            "};\n");
    return 0;
}

int main(int argc, char *argv[])
{
    t_scope;
    const char *arg0 = NEXTARG(argc, argv);
    char reldir[PATH_MAX];
    char *tmp_filepath = NULL;
    FILE *in = stdin, *out = stdout, *deps = NULL;

    argc = parseopt(argc, argv, popt, 0);
    if (argc < 0 || argc > 1 || opts_g.help)
        makeusage(EXIT_FAILURE, arg0, "<farch-script>", NULL, popt);

    if (opts_g.out) {
        /* XXX: using a temporary file is a hack: the build system is
         *      simultaneously calling farchc twice on iopc.fc,
         *      leading to unpredictable results. */
        tmp_filepath = t_fmt("%s.farchc.%d.%d.tmp", opts_g.out, getpid(),
                             (int)time(NULL));
        out = fopen(tmp_filepath, "w");
        DIE_IF(!out, "unable to open `%s` for writing: %m", opts_g.out);
    }

    if (argc > 0) {
        in = fopen(argv[0], "r");
        path_dirname(reldir, sizeof(reldir), argv[0]);
        path_join(reldir, sizeof(reldir), "/");
        DIE_IF(!in, "unable to open `%s` for reading: %m", argv[0]);
    } else {
        reldir[0] = '\0';
    }

    if (opts_g.deps) {
        deps = fopen(opts_g.deps, "w");
        DIE_IF(!deps, "unable to open `%s` for writing: %m", opts_g.deps);
    }

    do_work(reldir, in, out, deps);
    p_fclose(&in);
    p_fclose(&out);
    p_fclose(&deps);
    if (opts_g.out) {
        if (rename(tmp_filepath, opts_g.out) < 0) {
            unlink(tmp_filepath);
            e_error("farchc can't rename `%s` to `%s`: %m.",
                    tmp_filepath, opts_g.out);
            return 1;
        }
        DIE_IF(chmod(opts_g.out, 0440), "unable to chmod `%s`: %m",
               opts_g.out);
    }
    TRACE("OK !");
    return 0;
}
