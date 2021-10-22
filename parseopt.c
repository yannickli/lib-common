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

#include <sysexits.h>

#include "parseopt.h"

#define FLAG_SHORT 1
#define FLAG_UNSET 2

typedef struct popt_state_t {
    char **argv;
    int argc;
    const char *p;
} popt_state_t;

static const char *opt_arg(popt_state_t *st)
{
    if (st->p) {
        const char *res = st->p;
        st->p = NULL;
        return res;
    }
    st->argc--;
    return *++st->argv;
}

static int opterror(popt_t *opt, const char *reason, int flags)
{
    if (flags & FLAG_SHORT) {
        e_error("option `%c' %s", opt->shrt, reason);
    } else
    if (flags & FLAG_UNSET) {
        e_error("option `no-%s' %s", opt->lng, reason);
    } else {
        e_error("option `%s' %s", opt->lng, reason);
    }
    return -1;
}

static int put_int_value(popt_t *opt, uint64_t v)
{
    switch (opt->int_vsize) {
#define CASE(_sc) case _sc / 8:                                              \
        if (opt->kind == OPTION_UINT) {                                      \
            if (v <= UINT##_sc##_MAX) {                                      \
                *(uint##_sc##_t *)opt->value = v;                            \
            } else {                                                         \
                return -1;                                                   \
            }                                                                \
        } else {                                                             \
            int64_t w = (int64_t)v;                                          \
            if (w >= INT##_sc##_MIN && w <= INT##_sc##_MAX) {                \
                *(int##_sc##_t *)opt->value = v;                             \
            } else {                                                         \
                return -1;                                                   \
            }                                                                \
        }                                                                    \
        break;

        CASE(8);
        CASE(16);
        CASE(32);
        CASE(64);

#undef CASE

      default: e_panic("should not happen");
    }

    return 0;
}

static int get_value(popt_state_t *st, popt_t *opt, int flags)
{
    if (st->p && (flags & FLAG_UNSET))
        return opterror(opt, "takes no value", flags);

    switch (opt->kind) {
        const char *s;

      case OPTION_FLAG:
        if (!(flags & FLAG_SHORT) && st->p)
            return opterror(opt, "takes no value", flags);
        if (flags & FLAG_UNSET) {
            *(int *)opt->value = 0;
        } else {
            (*(int *)opt->value)++;
        }
        return 0;

      case OPTION_STR:
        if (flags & FLAG_UNSET) {
            *(const char **)opt->value = (const char *)opt->init;
        } else {
            if (!st->p && st->argc < 2)
                return opterror(opt, "requires a value", flags);
            *(const char **)opt->value = opt_arg(st);
        }
        return 0;

      case OPTION_CHAR:
        if (flags & FLAG_UNSET) {
            *(char *)opt->value = (char)opt->init;
        } else {
            const char *value;
            if (!st->p && st->argc < 2)
                return opterror(opt, "requires a value", flags);
            value = opt_arg(st);
            if (strlen(value) != 1)
                return opterror(opt, "expects a single character", flags);
            *(char *)opt->value = value[0];
        }
        return 0;

      case OPTION_INT:
      case OPTION_UINT:
      {
          uint64_t v;

          if (flags & FLAG_UNSET) {
              v = opt->init;
          } else {
              if (!st->p && st->argc < 2) {
                  return opterror(opt, "requires a value", flags);
              }

              errno = 0;
              if (opt->kind == OPTION_UINT) {
                  v = strtoull(opt_arg(st), &s, 10);
              } else {
                  v = strtoll(opt_arg(st), &s, 10);
              }
              if (*s || (errno && errno != ERANGE)) {
                  return opterror(opt, "expects a numerical value", flags);
              }
          }

          if (errno == ERANGE || put_int_value(opt, v) < 0) {
              return opterror(opt, "integer overflow", flags);
          }
      }
      return 0;

      case OPTION_VERSION:
        if (flags & FLAG_UNSET) {
            return opterror(opt, "takes no value", flags);
        }
        makeversion(EX_OK, opt->value, (void *)opt->init);

      default:
        e_panic("should not happen, programmer is a moron");
    }
}

static int parse_short_opt(popt_state_t *st, popt_t *opts)
{
    for (; opts->kind; opts++) {
        if (opts->shrt == *st->p) {
            st->p = st->p[1] ? st->p + 1 : NULL;
            return get_value(st, opts, FLAG_SHORT);
        }
    }
    return e_error("unknown option `%c'", *st->p);
}

static int parse_long_opt(popt_state_t *st, const char *arg, popt_t *opts)
{
    for (; opts->kind; opts++) {
        const char *p;
        int flags = 0;

        if (!opts->lng)
            continue;

        if (!strstart(arg, opts->lng, &p)) {
            if (!strstart(arg, "no-", &p) || !strstart(p, opts->lng, &p))
                continue;
            flags = FLAG_UNSET;
        }
        if (*p) {
            if (*p != '=')
                continue;
            st->p = p + 1;
        }
        return get_value(st, opts, flags);
    }
    return e_error("unknown option `%s'", arg);
}

static intptr_t get_int_init(popt_t *opt)
{
    switch (opt->int_vsize) {
#define CASE(_sc) case _sc / 8:                                              \
        if (opt->kind == OPTION_UINT) {                                      \
            return *(uint##_sc##_t *)opt->value;                             \
        } else {                                                             \
            return *(int##_sc##_t *)opt->value;                              \
        }                                                                    \
        break

        CASE(8);
        CASE(16);
        CASE(32);
        CASE(64);

#undef CASE

      default: e_panic("should not happen");
    }

    return 0;
}

static void copyinits(popt_t *opts)
{
    for (;;) {
        switch (opts->kind) {
          case OPTION_INT:
          case OPTION_UINT:
            opts->init = get_int_init(opts);
            break;
          case OPTION_STR:
            opts->init = (intptr_t)*(const char **)opts->value;
            break;
          case OPTION_CHAR:
            opts->init = *(char *)opts->value;
            break;
          case OPTION_FLAG:
          case OPTION_GROUP:
          case OPTION_VERSION:
            break;
          case OPTION_END:
            return;
        }
        opts++;
    }
}

int parseopt(int argc, char **argv, popt_t *opts, int flags)
{
    popt_state_t optst = { argv, argc, NULL };
    int n = 0;

    copyinits(opts);

    for (; optst.argc > 0; optst.argc--, optst.argv++) {
        const char *arg = optst.argv[0];

        if (*arg != '-' || !arg[1]) {
            if (flags & POPT_STOP_AT_NONARG)
                break;
            argv[n++] = optst.argv[0];
            continue;
        }

        if (arg[1] != '-') {
            optst.p = arg + 1;
            do {
                RETHROW(parse_short_opt(&optst, opts));
            } while (optst.p);
            continue;
        }

        if (!arg[2]) { /* "--" */
            optst.argc--;
            optst.argv++;
            break;
        }

        RETHROW(parse_long_opt(&optst, arg + 2, opts));
    }

    memmove(argv + n, optst.argv, optst.argc * sizeof(*argv));
    return n + optst.argc;
}

#define OPTS_WIDTH 20
#define OPTS_GAP    2

void makeusage(int ret, const char *arg0, const char *usage,
               const char * const text[], popt_t *opts)
{
    const char *p = strrchr(arg0, '/');

    printf("Usage: %s [options] %s\n", p ? p + 1 : arg0, usage);
    if (text) {
        putchar('\n');
        while (*text) {
            printf("    %s\n", *text++);
        }
    }
    if (opts->kind != OPTION_GROUP)
        putchar('\n');
    for (; opts->kind; opts++) {
        int pos = 4;

        if (opts->kind == OPTION_GROUP) {
            putchar('\n');
            if (*opts->help)
                printf("%s\n", opts->help);
            continue;
        }
        printf("    ");
        if (opts->shrt) {
            pos += printf("-%c", opts->shrt);
        }
        if (opts->lng) {
            pos += printf(opts->shrt ? ", --%s" : "--%s", opts->lng);
        }
        if (opts->kind != OPTION_FLAG) {
            pos += printf(" ...");
        }
        if (pos <= OPTS_WIDTH) {
            printf("%*s%s\n", OPTS_WIDTH + OPTS_GAP - pos, "", opts->help);
        } else {
            printf("\n%*s%s\n", OPTS_WIDTH + OPTS_GAP, "", opts->help);
        }
    }
    exit(ret);
}

void makeversion(int ret, const char *name, const char *(*get_version)(void))
{
    if (name && get_version) {
        printf("Intersec %s\n"
               "Revision: %s\n",
               name, (*get_version)());
    } else {
        int main_versions_printed = 0;

        for (int i = 0; i < core_versions_nb_g; i++) {
            const core_version_t *version = &core_versions_g[i];

            if (version->is_main_version) {
                printf("Intersec %s %s\n"
                       "Revision: %s\n",
                       version->name, version->version,
                       version->git_revision);
                main_versions_printed++;
            }
        }
        if (main_versions_printed > 0) {
            printf("\n");
        }
        for (int i = 0; i < core_versions_nb_g; i++) {
            const core_version_t *version = &core_versions_g[i];

            if (!version->is_main_version) {
                printf("%s %s (%s)\n",
                       version->name, version->version,
                       version->git_revision);
            }
        }
    }

    printf("\n"
           "See http://www.intersec.com/ for more details about our\n"
           "line of products for telecommunications operators\n");
    exit(ret);
}
