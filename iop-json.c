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

#include <math.h>
#include "unix.h"
#include "iop.h"
#include "iop-helpers.inl.c"

/* {{{ lexing json */

enum {
    IOP_JSON_EOF     = 0,
    /* plus { } [ ] : , */
    IOP_JSON_IDENT   = 128,
    IOP_JSON_INTEGER,
    IOP_JSON_DOUBLE,
    IOP_JSON_STRING,
};

/* Lexing helpers macros */
#define PS              ll->ps
#define SKIP(l) \
    ({ int tmp = l; ll->ctx->col += tmp; __ps_skip(PS, tmp); })
#define EATC()          (ll->ctx->col++, __ps_getc(PS))
#define READC()         (*PS->b)
#define READAT(off)     PS->b[off]
#define HAS(l)          ps_has(PS, l)
#define NEWLINE()       (ll->ctx->line++, ll->ctx->col = 1)

/* Context control */
#define STORECTX()      (ll->s_ps = *PS, ll->s_line = ll->ctx->line,        \
                         ll->s_col = ll->ctx->col)
#define RESTORECTX()    (ll->ctx->line = ll->s_line,                        \
                         ll->ctx->col = ll->s_col,                          \
                         *PS = ll->s_ps)

/* Errors throwing */
#define JERROR_MAXLEN   20
#define JERROR(_err)    (ll->err = (_err))
#define JERROR_WARG(_err, _len) \
    ({ jerror_strncpy(ll, PS->s, _len);                                     \
       JERROR(_err);                                                        \
    })

/* Errors throwing with context restoring */
#define RJERROR(_err)   (ll->ctx->line = ll->s_line,                        \
                         ll->ctx->col = ll->s_col,                          \
                         *PS = ll->s_ps, ll->err = (_err))
#define RJERROR_WARG(_err) \
    ({ __ps_clip_at(&ll->s_ps, PS->p);                                      \
       jerror_strncpy(ll, ll->s_ps.s, ps_len(&ll->s_ps));                   \
       RJERROR(_err);                                                       \
    })

/* Errors throwing with custom, constants args */
#define RJERROR_EXP(_exp) \
    rjerror_exp(ll, IOP_JERR_EXP_VAL, (_exp), strlen(_exp))
#define RJERROR_EXP_FMT(_exp, ...) \
    ({  char _buf[BUFSIZ];                                                  \
        int  _len = snprintf(_buf, BUFSIZ, (_exp), __VA_ARGS__);            \
        rjerror_exp(ll, IOP_JERR_EXP_VAL, _buf, _len);                      \
    })
#define RJERROR_EXP_TYPE(_type) \
    rjerror_exp(ll, IOP_JERR_EXP_VAL, iop_type_to_error_str(_type), -1)
#define RJERROR_SARG(_err, _str) \
    ({ const char *tmp = _str;                                              \
       ll->err_str = mp_dup(ll->mp, tmp, strlen(tmp) + 1);                  \
       RJERROR(_err);                                                       \
    })
#define RJERROR_SFIELD(_err, _s, _f) \
    ({ const iop_struct_t *_desc  = _s;                                     \
       const iop_field_t  *_fdesc = _f;                                     \
       ll->err_str = mp_new(ll->mp, char,                                   \
                            _desc->fullname.len + _fdesc->name.len + 2);    \
       sprintf(ll->err_str, "%s:%s", _desc->fullname.s, _fdesc->name.s);    \
       RJERROR(_err);                                                       \
    })

/* Constants checking */
#define IS_TRUE() \
    (  (ll->ctx->b.len == 4 && !memcmp(ll->ctx->b.data, "true", 4))         \
    || (ll->ctx->b.len == 3 && !memcmp(ll->ctx->b.data, "yes", 3)))
#define IS_FALSE() \
    (  (ll->ctx->b.len == 5 && !memcmp(ll->ctx->b.data, "false", 5))        \
    || (ll->ctx->b.len == 2 && !memcmp(ll->ctx->b.data, "no", 2)))
#define IS_NULL() \
    (  (ll->ctx->b.len == 4 && !memcmp(ll->ctx->b.data, "null", 4))         \
    || (ll->ctx->b.len == 3 && !memcmp(ll->ctx->b.data, "nil", 3)))

#define DO_IN_CTX(_ctx_tk, expr) \
    ({ typeof(expr) res;                                                    \
       iop_json_lex_ctx_t *stored_ctx = ll->ctx;                            \
                                                                            \
       ll->ctx = &ll->_ctx_tk##_ctx;                                        \
       res = (expr);                                                        \
       ll->ctx = stored_ctx;                                                \
       res;                                                                 \
    })


static int
werror_sb(void *sb, int len, const char *fmt, ...) __attr_printf__(3, 4);
static int
werror_buf(void *buf, int len, const char *fmt, ...) __attr_printf__(3, 4);
static int
iop_jlex_werror(iop_json_lex_t *ll, void *buf, int len,
                int (*writecb)(void *buf, int len, const char *fmt, ...)
                __attr_printf__(3, 4));

static int werror_sb(void *sb, int len, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    sb_addvf(sb, fmt, ap);
    va_end(ap);
    return 0;
}

static int werror_buf(void *buf, int len, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, len, fmt, ap);
    va_end(ap);
    return n;
}

static int
iop_jlex_werror(iop_json_lex_t *ll, void *buf, int len,
                int (*writecb)(void *buf, int len, const char *fmt, ...))
{
#define ESTR(fmt, ...) \
    (*writecb)(buf, len, "%d:%d: "fmt, ll->ctx->line, ll->ctx->col, ##__VA_ARGS__)

    switch (ll->err) {
      case IOP_JERR_EOF:
        return ESTR("end of file");
      case IOP_JERR_UNCLOSED_COMMENT:
        return ESTR("unclosed comment");
      case IOP_JERR_UNCLOSED_STRING:
        return ESTR("unclosed string");

      case IOP_JERR_TOO_BIG_INT:
        return ESTR("`%s' is too large an integer", ll->err_str);
      case IOP_JERR_PARSE_NUM:
        return ESTR("cannot parse number `%s'", ll->err_str);
      case IOP_JERR_BAD_INT_EXT:
        return ESTR("bad integer extension `%s'", ll->err_str);
      case IOP_JERR_OUT_OF_RANGE:
        return ESTR("number `%s' is out of range", ll->err_str);

      case IOP_JERR_EXP_SMTH:
        return ESTR("something was expected after `%s'", ll->err_str);
      case IOP_JERR_EXP_VAL:
        return ESTR("%s", ll->err_str);
      case IOP_JERR_BAD_TOKEN:
        return ESTR("unexpected token `%s'", ll->err_str);
      case IOP_JERR_INVALID_FILE:
        return ESTR("invalid file");

      case IOP_JERR_BAD_IDENT:
        return ESTR("invalid identifier `%s'", ll->err_str);
      case IOP_JERR_BAD_VALUE:
        return ESTR("invalid IOP value `%s' (neither a number nor a string)",
                    ll->err_str);
      case IOP_JERR_ENUM_VALUE:
        return ESTR("unknown enum value `%s'", ll->err_str);

      case IOP_JERR_DUPLICATED_MEMBER:
        return ESTR("member `%s' already seen", ll->err_str);
      case IOP_JERR_MISSING_MEMBER:
        return ESTR("member `%s' is missing", ll->err_str);
      case IOP_JERR_UNION_ARR:
        return ESTR("syntax `member.ufield' for union cannot be used with"
                    " a repeated field");
      case IOP_JERR_UNION_RESERVED:
        return ESTR("the syntax `member.field' is reserved for unions");
      case IOP_JERR_NOTHING_TO_READ:
        return ESTR("there is nothing to read");

      case IOP_JERR_CONSTRAINT:
        return ESTR("invalid field (ending at `%s'): %s", ll->err_str,
                    iop_get_err());

      case IOP_JERR_UNKNOWN:
      default:
        return ESTR("unknown error");
    }
#undef ESTR
}

void iop_jlex_write_error(iop_json_lex_t *ll, sb_t *sb)
{
    iop_jlex_werror(ll, sb, 0, &werror_sb);
}

int iop_jlex_write_error_buf(iop_json_lex_t *ll, char *buf, int len)
{
    return iop_jlex_werror(ll, buf, len, &werror_buf);
}

/* Format an error of type: "expected `foo', got `bar'. The len of the error
 * string is:
 *   len(the bad token)
 *   + len(expected expression)
 *   + len("expected ?, got `?'") <=> 17
 *   + 1 // for the \0
 */
static int
rjerror_exp(iop_json_lex_t *ll, int err, const char *expected, int len)
{
    char *dst;
    int buflen = 0, pos = 0, pslen;
    bool overflow = false;

    __ps_clip_at(&ll->s_ps, PS->p);
    pslen = ps_len(&ll->s_ps);

    if (pslen > JERROR_MAXLEN) {
        /* len = MAXLEN + sizeof("...") + sizeof("\"") */
        pslen    = JERROR_MAXLEN;
        buflen   = 3;
        if (ll->s_ps.s[0] == '"')
            buflen++;
        overflow = true;
    }

    if (len < 0)
        len = strlen(expected);

    buflen += pslen + len + 17 + 1;
    dst = ll->err_str = mp_new(ll->mp, char, buflen);

    pos += pstrcpy(dst, buflen, "expected ");
    pos += pstrcpy(dst + pos, buflen - pos, expected);
    pos += pstrcpy(dst + pos, buflen - pos, ", got `");
    pos += pstrcpymem(dst + pos, buflen - pos, ll->s_ps.s, pslen);
    if (overflow && ll->s_ps.s[0] == '"') {
        pos += pstrcpy(dst + pos, buflen - pos, "...\"");
    } else
    if (overflow) {
        pos += pstrcpy(dst + pos, buflen - pos, "...");
    }
    pos += pstrcpy(dst + pos, buflen - pos, "'");
    assert((buflen - pos) == 1);

    return RJERROR(err);
}

static void jerror_strncpy(iop_json_lex_t *ll, const char *str, int len)
{
    if (len > JERROR_MAXLEN) {
        /* len = MAXLEN + strlen("...") + strlen("\"") + 1 */
        ll->err_str = mp_new(ll->mp, char, JERROR_MAXLEN + 5);
        memcpy(ll->err_str, str, JERROR_MAXLEN);
        ll->err_str[JERROR_MAXLEN] = '.';
        ll->err_str[JERROR_MAXLEN + 1] = '.';
        ll->err_str[JERROR_MAXLEN + 2] = '.';
        /* If str is a string we close it */
        if (ll->err_str[0] == '"')
            ll->err_str[JERROR_MAXLEN + 3] = '"';
    } else {
        ll->err_str = mp_new(ll->mp, char, len + 1);
        memcpy(ll->err_str, str, len);
    }
}

static const char *
iop_type_to_error_str(iop_type_t type)
{
    switch (type) {
      case IOP_T_I8:  case IOP_T_U8:
      case IOP_T_I16: case IOP_T_U16:
      case IOP_T_I32: case IOP_T_U32:
      case IOP_T_I64: case IOP_T_U64:
        return "an integer value";
      case IOP_T_ENUM:
        return "an enum value";
      case IOP_T_BOOL:
        return "a boolean value";
      case IOP_T_DOUBLE:
        return "a decimal value";
      case IOP_T_DATA:
        return "a bytes value";
      case IOP_T_STRING:
      case IOP_T_XML:
        return "a string value";
      case IOP_T_UNION:
        return "a union";
      case IOP_T_STRUCT:
        return "a structure";
    }
    return NULL;
}

static int iop_json_skip_comment(iop_json_lex_t *ll)
{
    while (HAS(2)) {
        if (EATC() == '*' && READC() == '/') {
            SKIP(1);
            return 0;
        } else
        if (READC() == '\n') {
            SKIP(1);
            NEWLINE();
        }
    }

    return RJERROR(IOP_JERR_UNCLOSED_COMMENT);
}

static int iop_json_lex_token(iop_json_lex_t *ll)
{
    const char *start = PS->s;

    while (!ps_done(PS) && (isalnum(READC()) || READC() == '_'))
        SKIP(1);

    if (HAS(1)) {
        /* Check for the authorised tokens after an identifier */
        switch (READC()) {
          case ':': case '=': case ',': case ';': case '.':
          case '[': case ']': case '{': case '}':
            break;
          default:
            if (!isspace(READC())) {
                SKIP(1);
                return RJERROR_WARG(IOP_JERR_BAD_IDENT);
            }
        }
    }

    sb_set(&ll->ctx->b, start, PS->s - start);
    return IOP_JSON_IDENT;
}

static int iop_json_lex_number(iop_json_lex_t *ll)
{
    unsigned int pos;
    bool is_d_err;
    int i_res;
    uint64_t i;
    double d;
    const char *p, *pd, *pi;
    unsigned int d_count, i_count;

    /* we try to detect where the integer ends (i.e. where strto* functions
     * will stop reading). it does not matter if we go too far (e.g. with
     * '2+3+4' it will read the whole expression and not just '2' */
    for (pos = 0; pos < ps_len(PS); pos++) {
        if (isalnum(READAT(pos)))
            continue;
        switch (READAT(pos)) {
          case '.': case '+': case '-':
            continue;
        }
        break;
    }

    /* in the unlikely case that the integer reaches the end of the PS,
     * we need a null-terminated buffer to pass to the strto* functions */
    if (unlikely(pos == ps_len(PS))) {
        sb_set(&ll->ctx->b, PS->b, ps_len(PS));
        p = ll->ctx->b.data;
    } else {
        p = PS->s;
    }

    errno = 0;
    d = strtod(p, (char **)&pd);
    is_d_err = errno;
    d_count = pd - p;

    if (*p == '-') {
        ll->ctx->is_signed = true;
        i_res = strtoll_ext(p, (int64_t *)&i, &pi, 0);
    } else {
        ll->ctx->is_signed = false;
        i_res = strtoull_ext(p, &i, &pi, 0);
    }

    i_count = pi - p;

    /* rationale: if we parse to the same point, it was an integer
     * and double _may_ lose precision on 64bits ints
     * and if i_count is more important than d_count in case of error
     * we return an integer error */
    if (d_count <= i_count) {
        is_d_err = true;
    }

    switch (is_d_err << 1 | (i_res < 0)) {
      case 0:
        /* FALLTHROUGH */

      case 1:
        if (unlikely(d_count > pos)) {
            e_trace(0, "strtod read further than us: %*pM vs %*pM",
                    d_count, PS->s, pos, PS->s);
        }
        ll->ctx->u.d = d;
        SKIP(d_count);
        return IOP_JSON_DOUBLE;

      case 2:
        if (unlikely(i_count > pos)) {
            e_trace(0, "strtoxll_ext read further than us: %*pM vs %*pM",
                    i_count , PS->s, pos, PS->s);
        }
        ll->ctx->u.i = i;
        SKIP(i_count);
        return IOP_JSON_INTEGER;

      default:
        if (errno == ERANGE)
            return JERROR_WARG(IOP_JERR_TOO_BIG_INT, pos);
        if (errno == EDOM)
            return JERROR_WARG(IOP_JERR_BAD_INT_EXT, pos);
    }
    return JERROR_WARG(IOP_JERR_PARSE_NUM, pos);
}

static int iop_json_lex_enum(iop_json_lex_t *ll, int terminator,
                             const iop_field_t *fdesc)
{
    bool found;
    const char *end;
    int len;
    lstr_t s;

    end = (const char *)memchr(PS->p, terminator, ps_len(PS));
    if (!end)
        return JERROR(IOP_JERR_UNCLOSED_STRING);
    len = end - PS->s;
    s = LSTR_INIT_V(PS->s, len);

    ll->ctx->u.i = iop_enum_from_lstr(fdesc->u1.en_desc, s, &found);
    if (!found)
        return JERROR_WARG(IOP_JERR_ENUM_VALUE, len);

    SKIP(len + 1);
    return IOP_JSON_INTEGER;
}

static int iop_json_lex_expr(iop_json_lex_t *ll, const iop_field_t *fdesc)
{
    uint64_t num = 0;
    int type = IOP_JSON_INTEGER;

    if (READC() == '+')
        SKIP(1);

    for(;;) {
        int c;

        while (!ps_done(PS) && isspace(READC())) {
            if (EATC() == '\n')
                NEWLINE();
        }
        switch (c = READC()) {

          case '<': case '>':
            SKIP(1);
            if (HAS(1) && READC() == c) {
                switch(c) {
                  case '<': c = CF_OP_LSHIFT; break;
                  case '>': c = CF_OP_RSHIFT; break;
                  default : break;
                }
            } else {
                return RJERROR_EXP("a bitwise shift operator");
            }
            /* FALLTHROUGH */
          case '-': case '+': case '/': case '~':
          case '&': case '|': case '%': case '^':
          case '(': case ')': case '*':
            if (c == '*' && HAS(2) && READAT(1) == '*') {
                SKIP(1);
                c = CF_OP_EXP;
            }
            if (c == '-' && iop_cfolder_empty(ll->cfolder)) {
                goto feed_number;
            }
            e_trace(1, "feed operator %c", c);
            if (iop_cfolder_feed_operator(ll->cfolder, c) < 0)
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            SKIP(1);
            break;

          case '.': case '0' ... '9':
          feed_number:
            type = RETHROW(iop_json_lex_number(ll));
            if (type == IOP_JSON_DOUBLE) {
                if (!iop_cfolder_empty(ll->cfolder)) {
                    e_warning("double value in an expression");
                    return RJERROR_EXP("a valid integer expression");
                }
                e_trace(1, "single double value %f", ll->ctx->u.d);
                return IOP_JSON_DOUBLE;
            }
            assert (type == IOP_JSON_INTEGER);
            e_trace(1, "feed number %jd", ll->ctx->u.i);
            if (iop_cfolder_feed_number(ll->cfolder, ll->ctx->u.i,
                                        ll->ctx->is_signed) < 0)
            {
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            }
            break;

          case '\'': case '"':
            if (!fdesc || fdesc->type != IOP_T_ENUM) {
                return RJERROR_EXP("an integer");
            }
            RETHROW(iop_json_lex_enum(ll, EATC(), fdesc));
            e_trace(1, "feed number %jd", ll->ctx->u.i);
            if (iop_cfolder_feed_number(ll->cfolder, ll->ctx->u.i,
                                        (ll->ctx->u.i < 0)) < 0)
            {
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            }
            break;

          default:
            goto result;
        }
    }

  result:
    /* Let's try to get a result */
    if (iop_cfolder_get_result(ll->cfolder, &num) < 0)
        return RJERROR_WARG(IOP_JERR_PARSE_NUM);

    ll->ctx->u.i = num;
    e_trace(1, "-> result %jd", ll->ctx->u.i);

    return IOP_JSON_INTEGER;
}

static int iop_json_lex_char(iop_json_lex_t *ll, int terminator)
{
    if (!HAS(2)) {
        return RJERROR_WARG(IOP_JERR_EXP_SMTH);
    }
    if (READAT(1) == terminator) {
        /* single character */
        ll->ctx->u.i = READAT(0);
        SKIP(2);
        return IOP_JSON_INTEGER;
    } else
    /* escape sequence */
    if (READAT(0) == '\\') {
#define OCTAL -1
#define UNICODE -2
#define HEXADECIMAL -3
        static int escaped[256] = {
            ['a'] = '\a',
            ['b'] = '\b',
            ['e'] = '\e',
            ['t'] = '\t',
            ['n'] = '\n',
            ['v'] = '\v',
            ['f'] = '\f',
            ['r'] = '\r',
            ['\\'] = '\\',
            ['"'] = '\"',
            ['\''] = '\'',
            ['0'] = OCTAL,
            ['1'] = OCTAL,
            ['2'] = OCTAL,
            ['u'] = UNICODE,
            ['x'] = HEXADECIMAL,
        };
        int c = escaped[READAT(1)];

        if (!HAS(2))
            return RJERROR_WARG(IOP_JERR_EXP_SMTH);
        switch (c) {
            int a, b;
          case OCTAL:
            if (HAS(4)
            &&  READAT(2) >= '0' && READAT(2) <= '7'
            &&  READAT(3) >= '0' && READAT(3) <= '7')
            {
                ll->ctx->u.i = ((READAT(1) - '0') << 6) |
                    ((READAT(2) - '0') << 3) | (READAT(3) - '0');
                SKIP(2);
            } else
            /* null */
            if (READAT(1) == '0') {
                ll->ctx->u.i = '\0';
            }
            break;
          case HEXADECIMAL:
            if (HAS(4) && (a = hexdecode(PS->s + 2)) >= 0) {
                ll->ctx->u.i = a;
                SKIP(2);
            }
            break;
          case UNICODE:
            if (HAS(6)
            &&  (a = hexdecode(PS->s + 2)) >= 0
            &&  (b = hexdecode(PS->s + 4)) >= 0)
            {
                ll->ctx->u.i = a << 8 | b;
                SKIP(4);
            }
            break;
          case 0:
            return RJERROR_WARG(IOP_JERR_EXP_SMTH);
          default:
            ll->ctx->u.i = c;
            break;
        }
        SKIP(2);
        if (READC() != terminator) {
            return RJERROR_WARG(IOP_JERR_EXP_SMTH);
        }
        SKIP(1);
        return IOP_JSON_INTEGER;
    } else {
        return RJERROR_WARG(IOP_JERR_EXP_SMTH);
    }
}

static int iop_json_lex_str(iop_json_lex_t *ll, int terminator)
{
    sb_reset(&ll->ctx->b);

    for (;;) {
        for (unsigned i = 0; i < ps_len(PS); i++) {
            if (READAT(i) == '\n') {
                return JERROR(IOP_JERR_UNCLOSED_STRING);
            } else
            if (READAT(i) == '\\') {
                sb_add(&ll->ctx->b, PS->p, i);
                SKIP(i);
                goto parse_bslash;
            } else
            if (READAT(i) == terminator) {
                sb_add(&ll->ctx->b, PS->p, i);
                SKIP(i + 1);
                return IOP_JSON_STRING;
            }
        }
        return JERROR(IOP_JERR_UNCLOSED_STRING);

      parse_bslash:
        if (!HAS(2))
            return RJERROR_WARG(IOP_JERR_EXP_SMTH);

        switch (READAT(1)) {
            int a, b;

          case 'a': case 'b': case 'e': case 't': case 'n': case 'v':
          case 'f': case 'r':
            sb_add_unquoted(&ll->ctx->b, PS->p, 2);
            SKIP(2);
            continue;
          case '\\': case '"': case '\'': case '/':
            sb_addc(&ll->ctx->b, READAT(1));
            SKIP(2);
            continue;
          case '0' ... '2':
            if (HAS(4)
                &&  READAT(2) >= '0' && READAT(2) <= '7'
                &&  READAT(3) >= '0' && READAT(3) <= '7')
            {
                sb_addc(&ll->ctx->b, ((READAT(1) - '0') << 6)
                        | ((READAT(2) - '0') << 3)
                        | (READAT(3) - '0'));
                SKIP(4);
                continue;
            }
            if (READAT(1) == '0') {
                sb_addc(&ll->ctx->b, '\0');
                SKIP(2);
                continue;
            }
            break;
          case 'x':
            if (HAS(4) && (a = hexdecode(PS->s + 2)) >= 0) {
                sb_addc(&ll->ctx->b, a);
                SKIP(4);
                continue;
            }
            break;
          case 'u':
            if (HAS(6) && (a = hexdecode(PS->s + 2)) >= 0
                && (b = hexdecode(PS->s + 4)) >= 0)
            {
                sb_adduc(&ll->ctx->b, (a << 8) | b);
                SKIP(6);
                continue;
            }
            break;
          case '\n':
            sb_add(&ll->ctx->b, PS->p, 2);
            SKIP(2);
            NEWLINE();
            continue;
        }
        sb_add(&ll->ctx->b, PS->p, 2);
        SKIP(2);
    }
}

static int iop_json_lex(iop_json_lex_t *ll, const iop_field_t *fdesc)
{
    if (ll->peek >= 0) {
        int res = ll->peek;
        ll->peek = -1;

        /* copy the peeked context */
        sb_setsb(&ll->cur_ctx.b, &ll->peeked_ctx.b);
        ll->cur_ctx.line      = ll->peeked_ctx.line;
        ll->cur_ctx.col       = ll->peeked_ctx.col;
        ll->cur_ctx.u         = ll->peeked_ctx.u;
        ll->cur_ctx.is_signed = ll->peeked_ctx.is_signed;

        return res;
    }

  trackback:
    while (!ps_done(PS) && isspace(READC())) {
        if (EATC() == '\n')
            NEWLINE();
    }

    if (ps_done(PS))
        return 0;

    STORECTX();
    switch (READC()) {
        int c;
      case '=': SKIP(1); return ':';
      case ';': SKIP(1); return ',';
      case ':': case ',':
      case '{': case '}':
      case '[': case ']':
      case '@':
                return EATC();

      case 'c': /* string character prefix */
                c = READAT(1);
                if (c == '\'' || c == '"') {
                    SKIP(2);
                    return iop_json_lex_char(ll, c);
                }
                /* FALLTHROUGH */
      case 'a' ... 'b': case 'd' ... 'z': case 'A' ... 'Z':
                return iop_json_lex_token(ll);

      case '.':
                if (!(HAS(2) && READAT(1) >= '0' && READAT(1) <= '9'))
                    return EATC();
                /* FALLTHROUGH */
      case '-': case '+': case '~':
      case '0' ... '9':
      case '(': case ')':
                return iop_json_lex_expr(ll, fdesc);

      case '\'': case '"':
                if (fdesc && fdesc->type == IOP_T_ENUM) {
                    return iop_json_lex_expr(ll, fdesc);
                }
                return iop_json_lex_str(ll, EATC());

      case '/':
                if (!HAS(2))
                    return JERROR_WARG(IOP_JERR_EXP_SMTH, 1);
                SKIP(1);
                c = EATC();

                if (c == '*') {
                    PS_CHECK(iop_json_skip_comment(ll));
                    goto trackback;
                } else
                if (c != '/') {
                    return RJERROR_EXP("/");
                }
                /* FALLTHROUGH */
      case '#':
                if (ps_skip_afterchr(PS, '\n') < 0) {
                    ll->ctx->col += ps_len(PS);
                    ps_skip(PS, ps_len(PS));
                    return 0;
                }
                NEWLINE();
                goto trackback;
      default:
                return JERROR_WARG(IOP_JERR_BAD_TOKEN, 1);
    }
}

static int iop_json_lex_peek(iop_json_lex_t *ll, const iop_field_t *fdesc)
{
    if (ll->peek < 0) {
        /* Change of parser context */
        ll->peeked_ctx.line = ll->cur_ctx.line;
        ll->peeked_ctx.col  = ll->cur_ctx.col;
        ll->ctx  = &ll->peeked_ctx;

        /* Peek the next token */
        ll->peek = iop_json_lex(ll, fdesc);

        /* Restore parser context */
        ll->ctx  = &ll->cur_ctx;
    }
    return ll->peek;
}


iop_json_lex_t *iop_jlex_init(mem_pool_t *mp, iop_json_lex_t *ll)
{
    p_clear(ll, 1);
    ll->mp = mp;
    sb_init(&ll->cur_ctx.b);
    sb_init(&ll->peeked_ctx.b);
    ll->cfolder = iop_cfolder_new();
    ll->ctx = &ll->cur_ctx;

    return ll;
}

void iop_jlex_wipe(iop_json_lex_t *ll)
{
    sb_wipe(&ll->cur_ctx.b);
    sb_wipe(&ll->peeked_ctx.b);
    iop_cfolder_delete(&ll->cfolder);
    if (ll->err_str)
        mp_delete(ll->mp, &ll->err_str);
}

static void iop_jlex_reset(iop_json_lex_t *ll)
{
    ll->peek    = -1;
    ll->s_line  = ll->ctx->line;
    ll->s_col   = ll->ctx->col;
    ll->err     = 0;
    ll->err_str = NULL;
    ll->s_ps    = *ll->ps;

    sb_reset(&ll->cur_ctx.b);
    sb_reset(&ll->peeked_ctx.b);
}

void iop_jlex_attach(iop_json_lex_t *ll, pstream_t *ps)
{
    ll->ps = ps;
    ll->ctx->line = 1;
    ll->ctx->col  = 1;
}

/*-}}}-*/
/* {{{ unpacking json way */

static int unpack_arr(iop_json_lex_t *, const iop_field_t *, void *);
static int unpack_union(iop_json_lex_t *, const iop_struct_t *, void *, bool);
static int unpack_struct(iop_json_lex_t *, const iop_struct_t *, void *,
                         bool);

static int skip_val(iop_json_lex_t *ll, bool in_arr)
{
    switch (PS_CHECK(iop_json_lex(ll, NULL))) {
      case IOP_JSON_IDENT:
        if (IS_TRUE() || IS_FALSE() || IS_NULL())
            return 0;
        return RJERROR_WARG(IOP_JERR_BAD_VALUE);

      case IOP_JSON_STRING:
      case IOP_JSON_DOUBLE:
      case IOP_JSON_INTEGER:
        return 0;

      case '[':
        return in_arr ? -1 : unpack_arr(ll, NULL, NULL);

      case '{':
        return unpack_struct(ll, NULL, NULL, false);

      case '@':
        return unpack_struct(ll, NULL, NULL, true);

      case '.':
        return unpack_union(ll, NULL, NULL, true);

      case IOP_JSON_EOF:
        return RJERROR_WARG(IOP_JERR_EXP_SMTH);

      default:
        return RJERROR_WARG(IOP_JERR_BAD_TOKEN);
    }
}

static int unpack_val(iop_json_lex_t *ll, const iop_field_t *fdesc,
                      void *value, bool in_arr)
{
    if (!in_arr && fdesc->repeat == IOP_R_REPEATED) {
        if (PS_CHECK(iop_json_lex(ll, NULL)) != '[')
            return RJERROR_EXP("`['");
        return unpack_arr(ll, fdesc, value);
    }

    switch (PS_CHECK(iop_json_lex(ll, fdesc))) {
      case IOP_JSON_IDENT:
        if (IS_TRUE()) {
            ll->ctx->u.i = 1;
            goto integer;
        } else
        if (IS_FALSE()) {
            ll->ctx->u.i = 0;
            goto integer;
        } else
        if (IS_NULL()) {
            switch (fdesc->type) {
              case IOP_T_STRING:
              case IOP_T_XML:
              case IOP_T_DATA:
                *(lstr_t *)value = LSTR_NULL_V;
                return 0;
              default:
                return RJERROR_EXP_TYPE(fdesc->type);
            }
        }
        return RJERROR_WARG(IOP_JERR_BAD_VALUE);

      case IOP_JSON_STRING:
        switch (fdesc->type) {
            const char *q;
            lstr_t *data;

          case IOP_T_I8:  case IOP_T_U8:
          case IOP_T_I16: case IOP_T_U16:
          case IOP_T_I32: case IOP_T_U32:
          case IOP_T_I64: case IOP_T_U64:
          case IOP_T_BOOL:
            if (IS_TRUE()) {
                ll->ctx->u.i = 1;
                goto integer;
            } else
            if (IS_FALSE()) {
                ll->ctx->u.i = 0;
                goto integer;
            }
            errno = 0;
            switch (fdesc->type) {
              case IOP_T_U8:
              case IOP_T_U16:
              case IOP_T_U32:
              case IOP_T_U64:
                ll->ctx->u.i = strtoull(ll->ctx->b.data, &q, 0);
                break;

              default:
                ll->ctx->u.i = strtoll(ll->ctx->b.data, &q, 0);
                break;
            }
            if (errno || q != (const char *)ll->ctx->b.data + ll->ctx->b.len)
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            goto integer;

          case IOP_T_DOUBLE:
            errno = 0;
            ll->ctx->u.d = strtod(ll->ctx->b.data, (char **)&q);
            if (errno || q != (const char *)ll->ctx->b.data + ll->ctx->b.len)
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            goto do_double;

          case IOP_T_DATA:
            data = (lstr_t *)value;
            if (ll->ctx->b.len == 0) {
                data->data = mp_new(ll->mp, char, 1);
                data->len  = 0;
            } else {
                sb_t  sb;
                int   blen = DIV_ROUND_UP(ll->ctx->b.len * 3, 4);
                char *buf  = mp_new_raw(ll->mp, char, blen + 1);

                sb_init_full(&sb, buf, 0, blen + 1, MEM_STATIC);
                if (sb_add_unb64(&sb, ll->ctx->b.data, ll->ctx->b.len)) {
                    mp_delete(ll->mp, &buf);
                    return RJERROR_WARG(IOP_JERR_BAD_VALUE);
                }
                data->data = buf;
                data->len  = sb.len;
            }
            return 0;

          case IOP_T_STRING:
          case IOP_T_XML:
            data = (lstr_t *)value;
            data->data = mp_dupz(ll->mp, ll->ctx->b.data, ll->ctx->b.len);
            data->len  = ll->ctx->b.len;
            return 0;

          default:
            return RJERROR_EXP_TYPE(fdesc->type);
        }

      case IOP_JSON_DOUBLE:
do_double:
        switch (fdesc->type) {
          case IOP_T_DOUBLE:
            *(double *)value = ll->ctx->u.d;
            return 0;
          default:
            return RJERROR_EXP_TYPE(fdesc->type);
        }

      case IOP_JSON_INTEGER:
        integer:
        switch (fdesc->type) {
#define CHECK_RANGE(_min, _max)  \
            do {                                                             \
                if (ll->ctx->u.i < _min || ll->ctx->u.i > _max) {            \
                    return RJERROR_WARG(IOP_JERR_OUT_OF_RANGE);              \
                }                                                            \
            } while (0)

          case IOP_T_DOUBLE:
            *(double *)value = ll->ctx->u.i;
            return 0;
          case IOP_T_I8:
            CHECK_RANGE(INT8_MIN, INT8_MAX);
            *(int8_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_U8:
            CHECK_RANGE(0, UINT8_MAX);
            *(uint8_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_I16:
            CHECK_RANGE(INT16_MIN, INT16_MAX);
            *(int16_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_U16:
            CHECK_RANGE(0, UINT16_MAX);
            *(uint16_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_ENUM:
          case IOP_T_I32:
            CHECK_RANGE(INT32_MIN, INT32_MAX);
            *(int32_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_U32:
            CHECK_RANGE(0, UINT32_MAX);
            *(uint32_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_I64: case IOP_T_U64:
            *(uint64_t *)value = ll->ctx->u.i;
            break;
          case IOP_T_BOOL:
            CHECK_RANGE(0, 1);
            *(bool *)value     = ll->ctx->u.i;
            return 0;
#undef CHECK_RANGE
          default:
            return RJERROR_EXP_TYPE(fdesc->type);
        }
        return 0;

      case '{':
        if (fdesc->type == IOP_T_STRUCT) {
            if (iop_field_is_class(fdesc)) {
                *(void **)value = NULL;
            }
            return unpack_struct(ll, fdesc->u1.st_desc, value, false);
        } else
        if (fdesc->type == IOP_T_UNION) {
            return unpack_union(ll, fdesc->u1.st_desc, value, false);
        }
        return RJERROR_EXP_TYPE(fdesc->type);

        /* Extended syntax of union */
      case '.':
        if (fdesc->type != IOP_T_UNION)
            return RJERROR(IOP_JERR_UNION_RESERVED);
        return unpack_union(ll, fdesc->u1.st_desc, value, true);

        /* Prefix extended syntax */
      case '@':
        if (fdesc->type != IOP_T_STRUCT)
            return RJERROR_EXP_TYPE(fdesc->type);
        if (iop_field_is_class(fdesc)) {
            *(void **)value = NULL;
        }
        return unpack_struct(ll, fdesc->u1.st_desc, value, true);

      case IOP_JSON_EOF:
        return RJERROR_WARG(IOP_JERR_EXP_SMTH);

      default:
        return RJERROR_EXP_TYPE(fdesc->type);
    }
}

static int unpack_arr(iop_json_lex_t *ll, const iop_field_t *fdesc,
                      void *value)
{
    lstr_t *arr = value;
    int size = 0;
    void *ptr;

    if (arr)
        p_clear(arr, 1);

    for (;;) {
        if (PS_CHECK(iop_json_lex_peek(ll, fdesc)) == ']') {
            iop_json_lex(ll, NULL);
            return 0;
        }
        if (fdesc) {
            if (arr->len >= size) {
                size = p_alloc_nr(size);
                arr->data = ll->mp->realloc(ll->mp, arr->data,
                                            arr->len * fdesc->size,
                                            size * fdesc->size, 0);
            }
            ptr = (void *)((char *)arr->data + arr->len * fdesc->size);
            PS_CHECK(unpack_val(ll, fdesc, ptr, true));
            arr->len++;
        } else {
            PS_CHECK(skip_val(ll, true));
        }
        switch(PS_CHECK(iop_json_lex(ll, NULL))) {
          case ',': break;
          case ']': return 0;
          default:  return RJERROR_EXP("`,', `;' or `]'");
        }
    }
}

/* unions have two syntaxes, depending where they are:
 *  outside of a struct: { selected_field: value }
 *  inside a struct, there is two alternatives:
 *      { ...
 *        umember: { selected_field: value };
 *        umember_arr: [ { sfield: val }, { sfield: val }, ... ];
 *        ...
 *      }
 *      or
 *      { ...
 *        umember.selected_field [:=] value;
 *        umember_arr: [ .sfield: val, .sfield: val, ... ];
 *        ...
 *      }
 *
 * XXX Note: skip_val works with unions only because without the extended
 * syntax a union can be handled like two overlapping structures.
 * skip_val calls this function when the extended syntax is detected.
 */
static int unpack_union(iop_json_lex_t *ll, const iop_struct_t *desc,
                        void* value, bool ext_syntax)
{
    const iop_field_t *fdesc = NULL;

    switch (PS_CHECK(iop_json_lex(ll, NULL))) {
      case IOP_JSON_IDENT:
      case IOP_JSON_STRING:
        if (desc) {
            if (__iop_field_find_by_name(desc, ll->ctx->b.data,
                                         ll->ctx->b.len, NULL, &fdesc) < 0)
            {
                return RJERROR_EXP("a valid union member name");
            }
        }
        break;
      default:
        return RJERROR_EXP("a valid member name");
    }

    switch (PS_CHECK(iop_json_lex(ll, NULL))) {
      case ':':
        if (fdesc) {
            /* Write the selected field */
            *((uint16_t *)value) = fdesc->tag;
            value = (char *)value + fdesc->data_offs;

            PS_CHECK(unpack_val(ll, fdesc, value, false));

            if (unlikely(iop_field_has_constraints(desc, fdesc))) {
                int ret = iop_field_check_constraints(desc, fdesc, value, 1,
                                                      false);
                if (ret < 0) {
                    return RJERROR_WARG(IOP_JERR_CONSTRAINT);
                }
            }

        } else {
            PS_CHECK(skip_val(ll, false));
        }
        break;

        /* Extended syntax of union */
      case '.':
        if (fdesc) {
            if (fdesc->type != IOP_T_UNION)
                return RJERROR(IOP_JERR_UNION_RESERVED);

            /* Write the selected field */
            *((uint16_t *)value) = fdesc->tag;

            PS_CHECK(unpack_union(ll, fdesc->u1.st_desc,
                                  (char *)value + fdesc->data_offs, true));
        } else {
            PS_CHECK(unpack_union(ll, NULL, NULL, true));
        }
        break;

      default:
        return RJERROR_EXP("`:' or `='");
    }

    /* With the json compliant syntax we must check for a `}' */
    if (!ext_syntax && PS_CHECK(iop_json_lex(ll, NULL)) != '}')
        return RJERROR_EXP("`}'");
    return 0;
}

static int
unpack_struct_prepare_class(iop_json_lex_t *ll, const iop_struct_t *desc,
                            const iop_struct_t **real_desc, void **value)
{
    const iop_struct_t *desc_it = desc;
    int nb_fields = desc->fields_len;

    /* Count the number of fields contained in the classes from the
     * wanted one to the master. */
    while (desc_it->class_attrs->parent) {
        desc_it = desc_it->class_attrs->parent;
        nb_fields += desc_it->fields_len;
    }

    if (*real_desc) {
        /* Real class type is already known (because "_class" field was not
         * found). */
        goto check;
    }

    /* Get the value of the "_class" field */
    if (PS_CHECK(iop_json_lex(ll, NULL)) != IOP_JSON_STRING) {
        RJERROR_EXP_TYPE(IOP_T_STRING);
        return -1;
    }

    /* Get the iop_struct_t of the instanciated class from its fullname */
    *real_desc = iop_get_class_by_fullname(desc_it, LSTR_SB_V(&ll->ctx->b));
    if (!*real_desc) {
        return RJERROR_EXP_FMT("a child of `%*pM'",
                               LSTR_FMT_ARG(desc->fullname));
    }

    /* We are trying to unpack a class of type "desc", and the packed
     * class is of type "real_desc". Check that this is authorized.
     *
     * Also count the fields contained in the classes from the instantiated
     * one to the wanted one (excluded). The total sum is the number of fields
     * of the instantiated type.
     */
  check:
    if ((*real_desc)->class_attrs->is_abstract) {
        return RJERROR_EXP("a non-abstract class");
    }
    desc_it = *real_desc;
    while (desc_it && desc_it != desc) {
        nb_fields += desc_it->fields_len;
        desc_it = desc_it->class_attrs->parent;
    }
    if (!desc_it) {
        return RJERROR_EXP_FMT("a child of `%*pM'",
                               LSTR_FMT_ARG(desc->fullname));
    }

    /* Allocate output value */
    if (*value) {
        *value = ll->mp->realloc(ll->mp, *value, 0, (*real_desc)->size,
                                 MEM_RAW);
    } else {
        *value = ll->mp->malloc(ll->mp, (*real_desc)->size, MEM_RAW);
    }

    /* Set the _vprt pointer */
    *(const iop_struct_t **)(*value) = *real_desc;

    return nb_fields;
}

static ALWAYS_INLINE void seen_free(uint32_t **ptr)
{
    if (unlikely(*ptr != NULL))
        free(*ptr);
}

/* If the prefixed argument is set to true, the function assumes that it has
 * to parse a value with the prefix syntax:
 *      @member value {
 *          ...
 *      }
 */
static int unpack_struct(iop_json_lex_t *ll, const iop_struct_t *desc,
                         void *value, bool prefixed)
{
    const iop_field_t *fdesc;
    uint32_t  seen_buf[BITS_TO_ARRAY_LEN(uint32_t, 256)];
    uint32_t *seen_alloc __attribute__((cleanup(seen_free))) = NULL;
    uint32_t *seen       = seen_buf;
    const iop_struct_t *real_desc = desc;
    bool is_class = false;

    pstream_t ctx_ps;
    int       ctx_line = 0, ctx_col = 0;
    bool      ctx_prefixed = prefixed;

#define INIT_SEEN(_len)  \
    do {                                                                     \
        size_t _count = BITS_TO_ARRAY_LEN(uint32_t, _len);                   \
                                                                             \
        if (unlikely(_len > bitsizeof(seen_buf))) {                          \
            seen = seen_alloc = p_new(uint32_t, _count);                     \
        } else {                                                             \
            p_clear(seen, _count);                                           \
        }                                                                    \
    } while (0)

    if (desc) {
        if ((is_class = iop_struct_is_class(desc))) {
            /* We are unpacking a class; first of all, we have to know its
             * real type, that is finding the "_class" field.
             * This will be done in the loop below. Store context so that we
             * can loop again of fields once the "_class" will be found.
             */
            real_desc = NULL;
            ctx_ps = *PS;
            ctx_line = ll->ctx->line;
            ctx_col  = ll->ctx->col;
        } else {
            /* We are unpacking a struct or a union, so we already know the
             * list of expected fields. */
            INIT_SEEN(desc->fields_len);
        }
    }

    for (;;) {
        bool found_class_field = false;
        bool skip_field        = false;

        fdesc = NULL;

        if (PS_CHECK(iop_json_lex_peek(ll, NULL)) == '}') {
            iop_json_lex(ll, NULL);
            break;
        }

        switch (PS_CHECK(iop_json_lex(ll, NULL))) {
          case IOP_JSON_IDENT:
          case IOP_JSON_STRING:
            if (!desc) {
                break;
            }
            if (real_desc) {
                int ifield;

                if (is_class) {
                    /* We are looping on the fields of a class and we
                     * already found the "_class" field. We have to skip
                     * it when reading it again. */
                    if (lstr_equal2(LSTR_SB_V(&ll->ctx->b),
                                    LSTR_IMMED_V("_class")))
                    {
                        skip_field = true;
                        break;
                    }
                }
                ifield = __iop_field_find_by_name(real_desc,
                                                  ll->ctx->b.data,
                                                  ll->ctx->b.len,
                                                  &desc, &fdesc);
                if (fdesc) {
                    if (TST_BIT(seen, ifield)) {
                        return RJERROR_SARG(IOP_JERR_DUPLICATED_MEMBER,
                                            fdesc->name.s);
                    }
                    SET_BIT(seen, ifield);
                } else {
                    if (!(ll->flags & IOP_UNPACK_IGNORE_UNKNOWN)) {
                        return RJERROR_EXP_FMT("field of struct %s",
                                               real_desc->fullname.s);
                    }
                }
            } else {
                /* We are looping on the fields of a class, looking for
                 * the "_class" one, which determines the real class type.
                 */
                if (lstr_equal2(LSTR_SB_V(&ll->ctx->b),
                                LSTR_IMMED_V("_class")))
                {
                    found_class_field = true;
                }
                skip_field = true;
            }
            break;

          default:
            return RJERROR_EXP("a valid member name");
        }

        if (fdesc && ll->flags & IOP_UNPACK_FORBID_PRIVATE) {
            const iop_field_attrs_t *attrs = iop_field_get_attrs(desc, fdesc);
            if (attrs && TST_BIT(&attrs->flags, IOP_FIELD_PRIVATE)) {
                return RJERROR_EXP("a valid member name");
            }
        }

        /* XXX `.' must be kept (using lex_peek) for the unpack_val
         * function */
        if (!prefixed && PS_CHECK(iop_json_lex_peek(ll, NULL)) != '.'
        &&  PS_CHECK(iop_json_lex(ll, NULL)) != ':')
        {
            return RJERROR_EXP("`:' or `='");
        }
        if (found_class_field) {
            int count;

          prepare_class:
            count = unpack_struct_prepare_class(ll, desc, &real_desc,
                                                (void **)value);
            PS_CHECK(count);
            INIT_SEEN((size_t)count);
            value = *(void **)value;
            /* The real class is now known; re-loop on the fields */
            *PS = ctx_ps;
            ll->ctx->line = ctx_line;
            ll->ctx->col  = ctx_col;
            prefixed = ctx_prefixed;
            continue;
        } else
        if (fdesc && !skip_field) {
            void *ptr = (char *)value + fdesc->data_offs;

            if (fdesc->repeat == IOP_R_OPTIONAL) {
                /* check if value is different of null */
                if (PS_CHECK(iop_json_lex_peek(ll, fdesc)) == IOP_JSON_IDENT)
                {
                    /* In case of optional field, the member value could be
                     * null, which is equivalent to an absent member */
                    if (DO_IN_CTX(peeked, IS_NULL())) {
                        iop_value_set_absent(fdesc, ptr);
                        /* consume the “null” token */
                        iop_json_lex(ll, NULL);
                        goto nextfield;
                    }
                }
                if (!iop_field_is_class(fdesc)) {
                    ptr = iop_value_set_here(ll->mp, fdesc, ptr);
                }
            }

            if (prefixed && (((1 << fdesc->type) & IOP_STRUCTS_OK)
                             || fdesc->repeat == IOP_R_REPEATED))
            {
                return RJERROR_EXP("a member with a scalar type");
            }
            PS_CHECK(unpack_val(ll, fdesc, ptr, false));

            if (unlikely(iop_field_has_constraints(desc, fdesc))) {
                int ret;

                if (fdesc->repeat == IOP_R_REPEATED) {
                    lstr_t *arr = ptr;

                    ret = iop_field_check_constraints(desc, fdesc, arr->data,
                                                      arr->len, false);
                } else {
                    ret = iop_field_check_constraints(desc, fdesc, ptr, 1,
                                                      false);
                }
                if (ret < 0) {
                    return RJERROR_WARG(IOP_JERR_CONSTRAINT);
                }
            }
        } else {
            PS_CHECK(skip_val(ll, false));
        }

      nextfield:
        if (prefixed) {
            /* Special handling of prefixed value */
            if (PS_CHECK(iop_json_lex(ll, NULL)) != '{')
                return RJERROR_EXP("`{'");

            prefixed = false;
            continue;
        }

        switch (PS_CHECK(iop_json_lex(ll, NULL))) {
          case ',':
            break;
          case '}':
            goto endcheck;
          default:
            return RJERROR_EXP("`,', `;' or `}'");
        }
    }
  endcheck:
    if (!desc) {
        return 0;
    }
    if (!real_desc) {
        /* "_class" field not found */
        if (desc->class_attrs->is_abstract) {
            /* This is mandatory for abstract classes */
            return RJERROR_EXP("`_class' field");
        } else {
            /* Consider it's of the expected type */
            real_desc = desc;
            goto prepare_class;
        }
    }

    /* Scan remaining fields */
    for (int i = 0;;) {
        int acc = i;

        fdesc = real_desc->fields;
        for (; i < acc + real_desc->fields_len; i++, fdesc++) {
            if (TST_BIT(seen, i))
                continue;
            if (__iop_skip_absent_field_desc(ll->mp, value, fdesc) < 0)
                return RJERROR_SFIELD(IOP_JERR_MISSING_MEMBER, real_desc,
                                      fdesc);
        }
        if (!is_class) {
            break;
        }
        if (!(real_desc = real_desc->class_attrs->parent)) {
            break;
        }
    }

#undef INIT_SEEN
    return 0;
}

/* If "desc" is a structure or a union, "value" is a pointer on the structure
 * to fill.
 * If "desc" is a class, "value" is a double-pointer on the structure to fill.
 * It will be (re)allocated when the size of the real class to unpack will be
 * known.
 */
int iop_junpack(iop_json_lex_t *ll, const iop_struct_t *desc, void *value,
                bool single_value)
{
    const char *src = PS->s;
    iop_jlex_reset(ll);
    switch (PS_CHECK(iop_json_lex(ll, NULL))) {
      case '{':
        if (desc->is_union) {
            PS_CHECK(unpack_union(ll, desc, value, false));
        } else {
            PS_CHECK(unpack_struct(ll, desc, value, false));
        }
        goto endcheck;
      case '@':
        if (desc->is_union) {
            return RJERROR_EXP("`{'");
        } else {
            PS_CHECK(unpack_struct(ll, desc, value, true));
        }

    endcheck:
        /* Skip trailing characters (insignificant characters) */
        for (;;) {
            switch (PS_CHECK(iop_json_lex(ll, NULL))) {
              case ',':
                continue;
              case IOP_JSON_EOF:
                break;
              default:
                /* The parser isn't allowed to consume this character, then
                 * we restore the parser context to its previous position */
                RESTORECTX();
            }
            break;
        }

        if (single_value) {
            if (iop_json_lex(ll, NULL) == IOP_JSON_EOF)
                return 0;
            return RJERROR_EXP("nothing at this point but");
        } else {
            return PS->s - src;
        }
        return 0;

      case IOP_JSON_EOF:
        if (!single_value)
            return 0;
        /* A union or a class can't be empty */
        if (desc->is_union || iop_struct_is_class(desc))
            return JERROR(IOP_JERR_NOTHING_TO_READ);

        /* At this point we check for any required field */
        for (int i = 0; i < desc->fields_len; i++) {
            if (__iop_skip_absent_field_desc(ll->mp, value,
                                             desc->fields + i) < 0)
            {
                return RJERROR_SFIELD(IOP_JERR_MISSING_MEMBER,
                                      desc, desc->fields + i);
            }
        }
        return 0;
      default:
        return RJERROR_EXP("`{'");
    }
}

int iop_junpack_ptr(iop_json_lex_t *ll, const iop_struct_t *st, void **out,
                    bool single_value)
{
    if (iop_struct_is_class(st)) {
        /* "out" will be (re)allocated after, when the real packed class type
         * will be known. */
        return iop_junpack(ll, st, out, single_value);
    }

    if (*out) {
        *out = ll->mp->realloc(ll->mp, *out, 0, st->size, MEM_RAW);
    } else {
        *out = ll->mp->malloc(ll->mp, st->size, MEM_RAW);
    }
    return iop_junpack(ll, st, *out, single_value);
}

#define CREATE_JUNPACK_PS(_fun, _data_type, _fun_to_call)  \
int _fun(pstream_t *ps, const iop_struct_t *desc, _data_type v,              \
         int flags, sb_t *errb)                                              \
{                                                                            \
    iop_json_lex_t  jll;                                                     \
    int res;                                                                 \
                                                                             \
    iop_jlex_init(t_pool(), &jll);                                           \
    iop_jlex_attach(&jll, ps);                                               \
    jll.flags = flags;                                                       \
                                                                             \
    if ((res = _fun_to_call(&jll, desc, v, true)) < 0) {                     \
        if (errb) {                                                          \
            iop_jlex_write_error(&jll, errb);                                \
        }                                                                    \
    }                                                                        \
                                                                             \
    iop_jlex_wipe(&jll);                                                     \
                                                                             \
    return res;                                                              \
}

CREATE_JUNPACK_PS(t_iop_junpack_ps,     void *,  iop_junpack)
CREATE_JUNPACK_PS(t_iop_junpack_ptr_ps, void **, iop_junpack_ptr)
#undef CREATE_JUNPACK_PS

#define CREATE_JUNPACK_FILE(_fun, _data_type, _fun_to_call)  \
int _fun(const char *filename, const iop_struct_t *st, _data_type out,       \
         int flags, sb_t *errb)                                              \
{                                                                            \
    int res;                                                                 \
    pstream_t ps;                                                            \
    lstr_t file = LSTR_NULL_V;                                               \
                                                                             \
    if (lstr_init_from_file(&file, filename, PROT_READ, MAP_SHARED) < 0) {   \
        if (errb) {                                                          \
            sb_addf(errb, "cannot read file %s: %m", filename);              \
        }                                                                    \
        res = IOP_JERR_INVALID_FILE;                                         \
        goto end;                                                            \
    }                                                                        \
                                                                             \
    ps = ps_initlstr(&file);                                                 \
    res = _fun_to_call(&ps, st, out, flags, errb);                           \
                                                                             \
 end:                                                                        \
    lstr_wipe(&file);                                                        \
    return res;                                                              \
}

CREATE_JUNPACK_FILE(t_iop_junpack_file,     void *,  t_iop_junpack_ps)
CREATE_JUNPACK_FILE(t_iop_junpack_ptr_file, void **, t_iop_junpack_ptr_ps)
#undef CREATE_JUNPACK_FILE

/*-}}}-*/
/* {{{ jpack */

static inline const void *
get_n_and_ptr(const iop_field_t *fdesc, const void *value, int *n)
{
    void *ptr = (char *)value + fdesc->data_offs;

    switch (fdesc->repeat) {
      case IOP_R_OPTIONAL:
        *n = iop_value_has(fdesc, ptr);
        if (*n && ((1 << fdesc->type) & IOP_STRUCTS_OK))
            ptr = *(void **)ptr;
        break;
      case IOP_R_REQUIRED:
      case IOP_R_DEFVAL:
        *n = 1;
        break;
      case IOP_R_REPEATED:
        *n  = ((lstr_t *)ptr)->len;
        ptr = ((lstr_t *)ptr)->data;
        break;
      default:
        abort();
    }
    return ptr;
}

static int do_write(int (*writecb)(void *, const void *, int),
                    void *priv, const void *_buf, int len)
{
    const uint8_t *buf = _buf;
    int pos = 0;

    while (pos < len) {
        int res = (*writecb)(priv, buf + pos, len - pos);

        if (res < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        pos += res;
    }
    return len;
}

static int do_indent(int (*writecb)(void *, const void *, int),
                     void *priv, int lvl)
{
    static char const tabs[32] = {
        '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
        '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
        '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
        '\t', '\t', '\t', '\t', '\t', '\t', '\t', '\t',
    };
    int todo = lvl;

    while (todo > 0) {
        int res = (*writecb)(priv, tabs, MIN(countof(tabs), todo));
        if (res < 0) {
            if (ERR_RW_RETRIABLE(errno))
                continue;
            return -1;
        }
        todo -= res;
    }
    return lvl;
}

static int
pack_txt(const iop_struct_t *desc, const void *value, int lvl,
         int (*writecb)(void *, const void *buf, int len),
         void *priv, unsigned flags);

static int __pack_txt(const iop_struct_t *desc, const void *value, int lvl,
                      int (*writecb)(void *, const void *buf, int len),
                      void *priv, unsigned flags, bool *first)
{
    /* ints:   sign, 20 digits, and NUL -> 22
       double: sign, digit, dot, 17 digits, e, sign, up to 3 digits NUL -> 25
     */
    char ibuf[25];
    int res = 0;

    const iop_field_t *fdesc, *fend;
    const bool with_indent = !(flags & IOP_JPACK_COMPACT);
    SB_8k(sb);

#define WRITE(b, l)    \
    do {                                                                   \
        int __res = do_write(writecb, priv, b, l);                         \
        if (__res < 0)                                                     \
            return -1;                                                     \
        res += __res;                                                      \
    } while (0)
#define PUTS(s)  WRITE(s, strlen(s))
#define IPUTS(s_with_indent, s_no_indent) \
    do {                                                                   \
        if (with_indent) {                                                 \
            PUTS(s_with_indent);                                           \
        } else {                                                           \
            PUTS(s_no_indent);                                             \
        }                                                                  \
    } while (0)

#define INDENT() \
    do {                                                                   \
        if (with_indent) {                                                 \
            int __res = do_indent(writecb, priv, lvl);                     \
            \
            if (__res < 0)                                                 \
                return -1;                                                 \
            res += __res;                                                  \
        }                                                                  \
    } while (0)
#define PUTU(i)  \
    WRITE(ibuf, sprintf(ibuf, "%llu", (unsigned long long)i))
#define PUTD(i)  \
    WRITE(ibuf, sprintf(ibuf, "%lld", (long long)i))

    if (desc->is_union) {
        fdesc = get_union_field(desc, value);
        fend  = fdesc + 1;
    } else {
        fdesc = desc->fields;
        fend  = desc->fields + desc->fields_len;
    }

    for (; fdesc < fend; fdesc++) {
        const void *ptr;
        bool repeated = fdesc->repeat == IOP_R_REPEATED;
        int n;

        if (flags & IOP_JPACK_SKIP_PRIVATE) {
            const iop_field_attrs_t *attrs = iop_field_get_attrs(desc, fdesc);
            if (attrs && TST_BIT(&attrs->flags, IOP_FIELD_PRIVATE))
                continue;
        }
        ptr = get_n_and_ptr(fdesc, value, &n);
        if (!n && !repeated)
            continue;

        if (*first) {
            *first = false;
        } else {
            IPUTS(",\n", ",");
        }

        if (!desc->is_union)
            INDENT();
        PUTS("\"");
        PUTS(fdesc->name.s);
        if (with_indent) {
            PUTS(repeated ? "\": [ " : "\": ");
        } else {
            PUTS(repeated ? "\":[" : "\":");
        }

        for (int j = 0; j < n; j++) {
            if (repeated && j)
                IPUTS(", ", ",");
            switch (fdesc->type) {
                const void *v;
                pstream_t ps;
                int len;

#define CASE(n) \
              case IOP_T_I##n: PUTD(IOP_FIELD(int##n##_t, ptr, j)); break;  \
              case IOP_T_U##n: PUTU(IOP_FIELD(uint##n##_t, ptr, j)); break
              CASE(8); CASE(16); CASE(32); CASE(64);
#undef CASE

              case IOP_T_ENUM:
                v = iop_enum_to_str(fdesc->u1.en_desc, IOP_FIELD(int, ptr, j)).s;
                if (likely(v)) {
                    PUTS("\""); PUTS(v); PUTS("\"");
                } else {
                    /* if not found, dump the integer */
                    PUTU(IOP_FIELD(int, ptr, j));
                }
                break;

              case IOP_T_BOOL:
                if (IOP_FIELD(bool, ptr, j)) {
                    PUTS("true");
                } else {
                    PUTS("false");
                }
                break;

              case IOP_T_DOUBLE:
                {
                    double d = IOP_FIELD(double, ptr, j);

                    if (isinf(d)) {
                        PUTS(d < 0 ? "-Infinity" : "Infinity");
                    } else {
                        WRITE(ibuf, sprintf(ibuf, "%.17e", d));
                    }
                }
                break;

              case IOP_T_UNION:
              case IOP_T_STRUCT:
                v = &IOP_FIELD(const char, ptr, j * fdesc->size);
                if (iop_field_is_class(fdesc)) {
                    /* Non-optional class fields have to be dereferenced
                     * (dereferencing of optional fields was already done just
                     *  above).
                     */
                    if (fdesc->repeat != IOP_R_OPTIONAL) {
                        v = *(void **)v;
                    }
                }
                len = RETHROW(pack_txt(fdesc->u1.st_desc, v, lvl, writecb, priv, flags));
                res += len;
                break;

              case IOP_T_STRING:
              case IOP_T_XML:
                ps = ps_init(IOP_FIELD(const lstr_t, ptr, j).s,
                             IOP_FIELD(const lstr_t, ptr, j).len);

                PUTS("\"");
                while (!ps_done(&ps)) {
                    /* r:32-127 -s:'\\"' */
                    static ctype_desc_t const json_safe_chars = { {
                        0x00000000, 0xfffffffb, 0xefffffff, 0xffffffff,
                        0x00000000, 0x00000000, 0x00000000, 0x00000000,
                    } };

                    const uint8_t *p = ps.b;
                    size_t nbchars;
                    int c;

                    nbchars = ps_skip_span(&ps, &json_safe_chars);
                    WRITE(p, nbchars);

                    if (ps_done(&ps))
                        break;
                    /* Assume broken utf-8 is mixed latin1 */
                    c = ps_getuc(&ps);
                    if (unlikely(c < 0))
                        c = ps_getc(&ps);
                    WRITE(ibuf, sprintf(ibuf, "\\u%04x", c));
                }
                PUTS("\"");
                break;

              case IOP_T_DATA:
                if (IOP_FIELD(const lstr_t, ptr, j).len) {
                    int dlen = IOP_FIELD(const lstr_t, ptr, j).len;

                    sb_reset(&sb);
                    sb_addc(&sb, '"');
                    sb_add_b64(&sb, IOP_FIELD(const lstr_t, ptr, j).data,
                               dlen, -1);
                    sb_addc(&sb, '"');
                    WRITE(sb.data, sb.len);
                } else {
                    PUTS("\"\"");
                }
                break;

              default:
                abort();
            }
        }
        if (repeated)
            IPUTS(" ]", "]");
    }

    return res;
}

static int
pack_txt(const iop_struct_t *desc, const void *value, int lvl,
         int (*writecb)(void *, const void *buf, int len),
         void *priv, unsigned flags)
{
    int res = 0;
    bool first = true;
    const bool with_indent = !(flags & IOP_JPACK_COMPACT);

    if (desc->is_union) {
        IPUTS("{ ", "{");
    } else {
        IPUTS("{\n", "{");
        lvl++;
    }

    if (iop_struct_is_class(desc)) {
        qv_t(iop_struct) parents;
        const iop_struct_t *real_desc = *(const iop_struct_t **)value;

        e_assert(panic, !real_desc->class_attrs->is_abstract,
                 "packing of abstract class '%*pM' is forbidden",
                 LSTR_FMT_ARG(real_desc->fullname));

        /* Write type of class */
        INDENT();
        IPUTS("\"_class\": ", "\"_class\":");
        PUTS("\"");
        PUTS(real_desc->fullname.s);
        PUTS("\"");
        first = false;

        /* We want to write the fields in the order "master -> children", and
         * not "children -> master", so first build a qvector of the parents.
         */
        qv_inita(iop_struct, &parents, 8);
        do {
            qv_append(iop_struct, &parents, real_desc);
        } while ((real_desc = real_desc->class_attrs->parent));

        /* Write fields of different levels */
        for (int pos = parents.len; pos-- > 0; ) {
            res += RETHROW(__pack_txt(parents.tab[pos], value, lvl, writecb,
                                      priv, flags, &first));
        }
        qv_wipe(iop_struct, &parents);

    } else {
        res += RETHROW(__pack_txt(desc, value, lvl, writecb, priv, flags,
                                  &first));
    }

    if (desc->is_union) {
        IPUTS(" }", "}");
    } else {
        if (!first && with_indent) {
            PUTS("\n");
        }
        lvl--;
        INDENT();
        PUTS("}");
    }

    if (lvl == 0 && !(flags & IOP_JPACK_NO_TRAILING_EOL)) {
        PUTS("\n");
    }

    return res;
}

#undef WRITE
#undef PUTS
#undef INDENT
#undef PUTU
#undef PUTI

int iop_jpack(const iop_struct_t *desc, const void *value,
              int (*writecb)(void *, const void *buf, int len),
              void *priv, unsigned flags)
{
    return pack_txt(desc, value, 0, writecb, priv, flags);
}

/* }}} */

struct jtrace {
    int lvl;
    int lno;
    const char *fname;
    const char *func;
    const char *name;
};

#ifndef NDEBUG
static int iop_jtrace_write(void *_b, const void *buf, int len)
{
    struct jtrace *jt = _b;

    e_trace_put_(jt->lvl, jt->fname, jt->lno, jt->func, jt->name,
                 "%*pM", len, buf);
    return len;
}

void iop_jtrace_(int lvl, const char *fname, int lno, const char *func,
                 const char *name, const iop_struct_t *st, const void *v)
{
    struct jtrace jt = {
        .lvl   = lvl,
        .lno   = lno,
        .fname = fname,
        .func  = func,
        .name  = name,
    };
    iop_jpack(st, v, iop_jtrace_write, &jt, false);
    e_trace_put_(lvl, fname, lno, func, name, "\n");
}
#endif
