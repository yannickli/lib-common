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
        if (flags & FLAG_UNSET) {
            *(int *)opt->value = opt->init;
        } else {
            if (!st->p && st->argc < 2)
                return opterror(opt, "requires a value", flags);
            *(int *)opt->value = strtoip(opt_arg(st), &s);
            if (*s)
                return opterror(opt, "expects a numerical value", flags);
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

static void copyinits(popt_t *opts)
{
    for (;;) {
        switch (opts->kind) {
          case OPTION_FLAG:
          case OPTION_INT:
            opts->init = *(int *)opts->value;
            break;
          case OPTION_STR:
            opts->init = (intptr_t)*(const char **)opts->value;
            break;
          case OPTION_CHAR:
            opts->init = *(char *)opts->value;
            break;
          default:
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
            optst.argc--, optst.argv++;
            break;
        }

        RETHROW(parse_long_opt(&optst, arg + 2, opts));
    }

    memmove(argv + n, optst.argv, optst.argc * sizeof(argv));
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
    printf("Intersec %s\n"
           "Revision: %s\n"
           "\n"
           "See http://www.intersec.com/ for more details about our\n"
           "line of products for telecommunications operators\n",
           name, (*get_version)());
    exit(ret);
}
