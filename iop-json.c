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

#include <math.h>
#include "unix.h"
#include "iop.h"
#include "iop-helpers.inl.c"

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
        *n  = ((iop_data_t *)ptr)->len;
        ptr = ((iop_data_t *)ptr)->data;
        break;
      default:
        abort();
    }
    return ptr;
}

/*----- lexing json -{{{-*/

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
    ({ int tmp = l; ll->col += tmp; __ps_skip(PS, tmp); })
#define EATC()          (ll->col++, __ps_getc(PS))
#define READC()         (*PS->b)
#define READAT(off)     PS->b[off]
#define HAS(l)          ps_has(PS, l)
#define NEWLINE()       (ll->line++, ll->col = 1)

/* Context control */
#define STORECTX()      (ll->s_ps = *PS, ll->s_line = ll->line,             \
                         ll->s_col = ll->col)
#define RESTORECTX()    (ll->line = ll->s_line, ll->col = ll->s_col,        \
                         *PS = ll->s_ps)

/* Errors throwing */
#define JERROR_MAXLEN   20
#define JERROR(_err)    (ll->err = (_err))
#define JERROR_WARG(_err, _len) \
    ({ jerror_strncpy(ll, PS->s, _len);                                     \
       JERROR(_err);                                                        \
    })

/* Errors throwing with context restoring */
#define RJERROR(_err)   (ll->line = ll->s_line, ll->col = ll->s_col,        \
                         *PS = ll->s_ps, ll->err = (_err))
#define RJERROR_WARG(_err) \
    ({ __ps_clip_at(&ll->s_ps, PS->p);                                      \
       jerror_strncpy(ll, ll->s_ps.s, ps_len(&ll->s_ps));                   \
       RJERROR(_err);                                                       \
    })

/* Errors throwing with custom, constants args */
#define RJERROR_EXP(_exp) \
    rjerror_exp(ll, IOP_JERR_EXP_VAL, (_exp), sizeof(_exp) - 1)
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
    (  (ll->b.len == 4 && !memcmp(ll->b.data, "true", 4))                   \
    || (ll->b.len == 3 && !memcmp(ll->b.data, "yes", 3)))
#define IS_FALSE() \
    (  (ll->b.len == 5 && !memcmp(ll->b.data, "false", 5))                  \
    || (ll->b.len == 2 && !memcmp(ll->b.data, "no", 2)))
#define IS_NULL() \
    (  (ll->b.len == 4 && !memcmp(ll->b.data, "null", 4))                   \
    || (ll->b.len == 3 && !memcmp(ll->b.data, "nil", 3)))

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
    (*writecb)(buf, len, "%d:%d: "fmt, ll->line, ll->col, ##__VA_ARGS__)

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

      case IOP_JERR_EXP_SMTH:
        return ESTR("something was expected after `%s'", ll->err_str);
      case IOP_JERR_EXP_VAL:
        return ESTR("%s", ll->err_str);
      case IOP_JERR_BAD_TOKEN:
        return ESTR("unexpected token `%s'", ll->err_str);

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

    sb_set(&ll->b, start, PS->s - start);
    return IOP_JSON_IDENT;
}

static int iop_json_lex_number_extensions(iop_json_lex_t *ll)
{
    uint64_t mult = 1;
    uint64_t u = ll->u.i;

    switch (READC()) {
        /* times */
      case 'w': mult *= 7;
      case 'd': mult *= 24;
      case 'h': mult *= 60;
      case 'm': mult *= 60;
      case 's': mult *= 1;
                SKIP(1);
                break;

                /* sizes */
      case 'T': mult *= 1024;
      case 'G': mult *= 1024;
      case 'M': mult *= 1024;
      case 'K': mult *= 1024;
                SKIP(1);
                break;

      default:
                /* Only spaces, ',' and ';' are allowed after an integer */
                if (isspace(READC()) || READC() == ',' || READC() == ';'
                 || READC() == ']' || READC() == '}') {
                    return IOP_JSON_INTEGER;
                }
                return JERROR_WARG(IOP_JERR_BAD_INT_EXT, 1);
    }

    if (u > UINT64_MAX / mult || u * mult < u)
        return RJERROR_WARG(IOP_JERR_TOO_BIG_INT);

    ll->u.i = u * mult;
    return IOP_JSON_INTEGER;
}

static int iop_json_lex_number(iop_json_lex_t *ll)
{
    unsigned int pos = 0;
    int d_err, i_err;
    long long i;
    double d;
    const char *p, *pd, *pi;
    bool is_hexa = false;

    while (pos < ps_len(PS)) {
        switch (READAT(pos)) {
          case '0'...'9': case 'e': case'E':
          case '.': case '+': case '-':
            pos++;
            continue;
          case 'x':
            is_hexa = true;
            pos++;
            continue;
          case 'a'...'d': case 'f':
          case 'A'...'D': case 'F':
            if (!is_hexa)
                break;
            pos++;
            continue;
          default:
            break;
        }
        break;
    }

    if (pos == ps_len(PS)) {
        sb_set(&ll->b, PS->b, ps_len(PS));
        p = ll->b.data;
    } else {
        p = PS->s;
    }

    errno = 0;
    d = strtod(p, (char **)&pd);
    d_err = errno;
    errno = 0;
    if (*p == '-') {
        i = strtoll(p, &pi, 0);
    } else {
        i = strtoull(p, &pi, 0);
    }
    i_err = errno;

    switch ((i_err != 0) | ((d_err != 0) << 1)) {
      case 0:
        /* rationale: if we parse to the same point, it was an integer
           and double _may_ lose precision on 64bits ints */
        if (pd <= pi)
            goto prefer_integer;
        /* FALLTHROUGH */

      case 1:
        if ((unsigned int)(pd - p) != pos)
            return JERROR_WARG(IOP_JERR_PARSE_NUM, pos);
        ll->u.d = d;
        SKIP(pos);
        /* Only spaces, ',' and ';' are allowed after a number */
        if (HAS(1) && !isspace(READC()) && READC() != ',' && READC() != ';') {
            SKIP(1);
            return RJERROR_WARG(IOP_JERR_PARSE_NUM);
        }
        return IOP_JSON_DOUBLE;

      case 2:
prefer_integer:
        if ((unsigned int)(pi - p) != pos)
            return JERROR_WARG(IOP_JERR_PARSE_NUM, pos);
        ll->u.i = i;
        SKIP(pos);
        if (HAS(1))
            return iop_json_lex_number_extensions(ll);
        return IOP_JSON_INTEGER;

      default:
        return JERROR_WARG(IOP_JERR_PARSE_NUM, pos);
    }
}

static int iop_json_lex_str(iop_json_lex_t *ll, int terminator)
{
    sb_reset(&ll->b);

    for (;;) {
        for (unsigned i = 0; i < ps_len(PS); i++) {
            if (READAT(i) == '\n') {
                return JERROR(IOP_JERR_UNCLOSED_STRING);
            } else
            if (READAT(i) == '\\') {
                sb_add(&ll->b, PS->p, i);
                SKIP(i);
                goto parse_bslash;
            } else
            if (READAT(i) == terminator) {
                sb_add(&ll->b, PS->p, i);
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
          case 'f': case 'r': case '\\': case '"': case '\'':
            sb_add_unquoted(&ll->b, PS->p, 2);
            SKIP(2);
            continue;
          case '0' ... '2':
            if (HAS(4)
                &&  READAT(2) >= '0' && READAT(2) <= '7'
                &&  READAT(3) >= '0' && READAT(3) <= '7')
            {
                sb_addc(&ll->b, ((READAT(1) - '0') << 6)
                        | ((READAT(2) - '0') << 3)
                        | (READAT(3) - '0'));
                SKIP(4);
                continue;
            }
            if (READAT(1) == '0') {
                sb_addc(&ll->b, '\0');
                SKIP(2);
                continue;
            }
            break;
          case 'x':
            if (HAS(4) && (a = hexdecode(PS->s + 2)) >= 0) {
                sb_addc(&ll->b, a);
                SKIP(4);
                continue;
            }
            break;
          case 'u':
            if (HAS(6) && (a = hexdecode(PS->s + 2)) >= 0
                && (b = hexdecode(PS->s + 4)) >= 0)
            {
                sb_adduc(&ll->b, (a << 8) | b);
                SKIP(6);
                continue;
            }
            break;
          case '\n':
            sb_add(&ll->b, PS->p, 2);
            SKIP(2);
            NEWLINE();
            continue;
        }
        sb_add(&ll->b, PS->p, 2);
        SKIP(2);
    }
}

static int iop_json_lex(iop_json_lex_t *ll)
{
    if (ll->peek >= 0) {
        int res = ll->peek;
        ll->peek = -1;
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

      case 'a' ... 'z': case 'A' ... 'Z':
                return iop_json_lex_token(ll);

      case '.':
                if (HAS(2) && READAT(1) >= '0' && READAT(1) <= '9')
                    return iop_json_lex_number(ll);
                else
                    return EATC();

      case '-': case '+':
      case '0' ... '9':
                return iop_json_lex_number(ll);

      case '\'': case '"':
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
                    ll->col += ps_len(PS);
                    ps_shrink(PS, 0);
                    return 0;
                }
                NEWLINE();
                goto trackback;
      default:
                return JERROR_WARG(IOP_JERR_BAD_TOKEN, 1);
    }
}

static int iop_json_lex_peek(iop_json_lex_t *ll)
{
    if (ll->peek < 0)
        ll->peek = iop_json_lex(ll);
    return ll->peek;
}


iop_json_lex_t *iop_jlex_init(mem_pool_t *mp, iop_json_lex_t *ll)
{
    p_clear(ll, 1);
    ll->mp = mp;
    sb_init(&ll->b);
    return ll;
}

void iop_jlex_wipe(iop_json_lex_t *ll)
{
    sb_wipe(&ll->b);
    if (ll->err_str)
        mp_delete(ll->mp, &ll->err_str);
}

static void iop_jlex_reset(iop_json_lex_t *ll)
{
    ll->peek    = -1;
    ll->s_line  = ll->line;
    ll->s_col   = ll->col;
    ll->err     = 0;
    ll->err_str = NULL;
    ll->s_ps    = *ll->ps;

    sb_reset(&ll->b);
}

void iop_jlex_attach(iop_json_lex_t *ll, pstream_t *ps)
{
    ll->ps = ps;
    ll->line = 1;
    ll->col  = 1;
}

/*-}}}-*/
/*----- unpacking json way -{{{-*/

static int
find_field_by_name(const iop_struct_t *desc, const void *s, int len)
{
    const iop_field_t *field = desc->fields;
    for (int i = 0; i < desc->fields_len; i++) {
        if (len == field->name.len && !memcmp(field->name.s, s, len))
            return i;
        field++;
    }
    return -1;
}

static int unpack_arr(iop_json_lex_t *, const iop_field_t *, void *);
static int unpack_union(iop_json_lex_t *, const iop_struct_t *, void *, bool);
static int unpack_struct(iop_json_lex_t *, const iop_struct_t *, void *,
                         bool);

static int skip_val(iop_json_lex_t *ll, bool in_arr)
{
    switch (PS_CHECK(iop_json_lex(ll))) {
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
        if (PS_CHECK(iop_json_lex(ll)) != '[')
            return RJERROR_EXP("`['");
        return unpack_arr(ll, fdesc, value);
    }

    switch (PS_CHECK(iop_json_lex(ll))) {
      case IOP_JSON_IDENT:
        if (IS_TRUE()) {
            ll->u.i = 1;
            goto integer;
        } else
        if (IS_FALSE()) {
            ll->u.i = 0;
            goto integer;
        } else
        if (IS_NULL()) {
            switch (fdesc->type) {
              case IOP_T_STRING:
              case IOP_T_XML:
                *(lstr_t *)value = LSTR_NULL_V;
                return 0;
              case IOP_T_DATA:
                *(iop_data_t *)value = IOP_DATA_NULL;
                return 0;
              default:
                return RJERROR_EXP_TYPE(fdesc->type);
            }
        }
        return RJERROR_WARG(IOP_JERR_BAD_VALUE);

      case IOP_JSON_STRING:
        switch (fdesc->type) {
            const iop_enum_t *edesc;
            const char *q;
            iop_data_t *data;

          case IOP_T_I8:  case IOP_T_U8:
          case IOP_T_I16: case IOP_T_U16:
          case IOP_T_I32: case IOP_T_U32:
          case IOP_T_I64: case IOP_T_U64:
          case IOP_T_BOOL:
            if (IS_TRUE()) {
                ll->u.i = 1;
                goto integer;
            } else
            if (IS_FALSE()) {
                ll->u.i = 0;
                goto integer;
            }
            errno = 0;
            ll->u.i = strtoll(ll->b.data, &q, 0);
            if (errno || q != (const char *)ll->b.data + ll->b.len)
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            goto integer;

          case IOP_T_ENUM:
            edesc = fdesc->u1.en_desc;
            for (int i = 0; i < edesc->enum_len; i++) {
                if (!strcasecmp(edesc->names[i].s, ll->b.data)) {
                    ll->u.i = edesc->values[i];
                    goto integer;
                }
            }
            return RJERROR_WARG(IOP_JERR_ENUM_VALUE);

          case IOP_T_DOUBLE:
            errno = 0;
            ll->u.d = strtod(ll->b.data, (char **)&q);
            if (errno || q != (const char *)ll->b.data + ll->b.len)
                return RJERROR_WARG(IOP_JERR_PARSE_NUM);
            goto do_double;

          case IOP_T_DATA:
            data = (iop_data_t *)value;
            if (ll->b.len == 0) {
                data->data = mp_new(ll->mp, char, 1);
                data->len  = 0;
            } else {
                sb_t  sb;
                int   blen = DIV_ROUND_UP(ll->b.len * 3, 4);
                char *buf  = mp_new_raw(ll->mp, char, blen + 1);

                sb_init_full(&sb, buf, 0, blen + 1, MEM_OTHER);
                if (sb_add_unb64(&sb, ll->b.data, ll->b.len))
                    return RJERROR_WARG(IOP_JERR_BAD_VALUE);
                data->data = buf;
                data->len  = sb.len;
            }
            return 0;

          case IOP_T_STRING:
          case IOP_T_XML:
            data = (iop_data_t *)value;
            data->data = mp_dupz(ll->mp, ll->b.data, ll->b.len);
            data->len  = ll->b.len;
            return 0;

          default:
            return RJERROR_EXP_TYPE(fdesc->type);
        }

      case IOP_JSON_DOUBLE:
do_double:
        switch (fdesc->type) {
          case IOP_T_DOUBLE:
            *(double *)value = ll->u.d;
            return 0;
          default:
            return RJERROR_EXP_TYPE(fdesc->type);
        }

      case IOP_JSON_INTEGER:
        integer:
        switch (fdesc->type) {
          case IOP_T_DOUBLE:
            *(double *)value = ll->u.i;
            return 0;
          case IOP_T_I8: case IOP_T_U8:
            *(uint8_t *)value = ll->u.i;
            break;
          case IOP_T_I16: case IOP_T_U16:
            *(uint16_t *)value = ll->u.i;
            break;
          case IOP_T_ENUM:
          case IOP_T_I32: case IOP_T_U32:
            *(uint32_t *)value = ll->u.i;
            break;
          case IOP_T_I64: case IOP_T_U64:
            *(uint64_t *)value = ll->u.i;
            break;
          case IOP_T_BOOL:
            *(bool *)value     = ll->u.i;
            return 0;
          default:
            return RJERROR_EXP_TYPE(fdesc->type);
        }
        return 0;

      case '{':
        if (fdesc->type == IOP_T_STRUCT) {
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
    iop_data_t *arr = value;
    int size = 0;
    void *ptr;

    if (arr)
        p_clear(arr, 1);

    for (;;) {
        if (PS_CHECK(iop_json_lex_peek(ll)) == ']') {
            iop_json_lex(ll);
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
        switch(PS_CHECK(iop_json_lex(ll))) {
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

    switch (PS_CHECK(iop_json_lex(ll))) {
      case IOP_JSON_IDENT:
      case IOP_JSON_STRING:
        if (desc) {
            int ifield = find_field_by_name(desc, ll->b.data, ll->b.len);

            if (ifield < 0)
                return RJERROR_EXP("a valid union member name");
            fdesc = desc->fields + ifield;
        }
        break;
      default:
        return RJERROR_EXP("a valid member name");
    }

    if (PS_CHECK(iop_json_lex(ll)) != ':')
        return RJERROR_EXP("`:' or `='");

    if (fdesc) {
        /* Write the selected field */
        *((uint16_t *)value) = fdesc->tag;

        PS_CHECK(unpack_val(ll, fdesc, (char *)value + fdesc->data_offs,
                            false));
    } else {
        PS_CHECK(skip_val(ll, false));
    }

    /* With the json compliant syntax we must check for a `}' */
    if (!ext_syntax && PS_CHECK(iop_json_lex(ll)) != '}')
        return RJERROR_EXP("`}'");
    return 0;
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
    bool *seen = NULL;

    if (desc)
        seen = mp_new(ll->mp, bool, desc->fields_len);

    for (;;) {
        fdesc = NULL;

        if (PS_CHECK(iop_json_lex_peek(ll)) == '}') {
            iop_json_lex(ll);
            break;
        }

        switch (PS_CHECK(iop_json_lex(ll))) {
          case IOP_JSON_IDENT:
          case IOP_JSON_STRING:
            if (desc) {
                int ifield = find_field_by_name(desc, ll->b.data, ll->b.len);
                if (ifield < 0) {
                    if (ll->warn_on_unkown_field) {
                        e_warning("no field named %s in struct %s",
                                ll->b.data, desc->fullname.s);
                    } else {
                        e_trace(0, "no field named %s in struct %s",
                                ll->b.data, desc->fullname.s);
                    }
                } else {
                    fdesc = desc->fields + ifield;
                    if (seen[ifield])
                        return RJERROR_SARG(IOP_JERR_DUPLICATED_MEMBER,
                                            fdesc->name.s);
                    seen[ifield] = true;
                }
            }
            break;

          default:
            return RJERROR_EXP("a valid member name");
        }
        /* XXX `.' must be kept (using lex_peek) for the unpack_val
         * function */
        if (!prefixed && PS_CHECK(iop_json_lex_peek(ll)) != '.'
        &&  PS_CHECK(iop_json_lex(ll)) != ':')
        {
            mp_delete(ll->mp, &seen);
            return RJERROR_EXP("`:' or `='");
        }
        if (fdesc) {
            void *ptr = (char *)value + fdesc->data_offs;
            if (fdesc->repeat == IOP_R_OPTIONAL) {
                ptr = iop_value_set_here(ll->mp, fdesc, ptr);
            }

            if (prefixed && (((1 << fdesc->type) & IOP_STRUCTS_OK)
                             || fdesc->repeat == IOP_R_REPEATED))
                return RJERROR_EXP("a member with a scalar type");
            PS_CHECK(unpack_val(ll, fdesc, ptr, false));
        } else {
            PS_CHECK(skip_val(ll, false));
        }

        if (prefixed) {
            /* Special handling of prefixed value */
            if (PS_CHECK(iop_json_lex(ll)) != '{')
                return RJERROR_EXP("`{'");

            prefixed = false;
            continue;
        }

        switch (PS_CHECK(iop_json_lex(ll))) {
          case ',':
            break;
          case '}':
            goto endcheck;
          default:
            mp_delete(ll->mp, &seen);
            return RJERROR_EXP("`,', `;' or `}'");
        }
    }
  endcheck:
    if (!desc)
        return 0;

    fdesc = desc->fields;
    for (int i = 0; i < desc->fields_len; i++) {
        if (!seen[i] && __iop_skip_absent_field_desc(value, fdesc) < 0) {
            mp_delete(ll->mp, &seen);
            return RJERROR_SFIELD(IOP_JERR_MISSING_MEMBER, desc, fdesc);
        }
        fdesc++;
    }
    mp_delete(ll->mp, &seen);
    return 0;
}

/** This function unpacks an iop structure from a json format. If you set the
 * flag `single_value` the function returns 0 on success and something < 0 on
 * error.  In particular, it returns IOP_JERR_NOTHING_TO_READ if EOF is
 * reached before reading anything. Reading nothing is not an error if
 * the structure can be empty.
 *
 * You should call iop_jlex_new and iop_jlex_attach before calling this
 * function. The pstream which is attached is consumed step by step.
 *
 * @return if the flag `single_value` is set to false, the function returns
 * something < 0 on error, otherwise it returns 0 if it reaches EOF before
 * reading anything or the number of bytes read successfully.
 */
int iop_junpack(iop_json_lex_t *ll, const iop_struct_t *desc, void *value,
                bool single_value)
{
    const char *src = PS->s;
    iop_jlex_reset(ll);
    switch (PS_CHECK(iop_json_lex(ll))) {
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
            switch (PS_CHECK(iop_json_lex(ll))) {
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
            if (iop_json_lex(ll) == IOP_JSON_EOF)
                return 0;
            return RJERROR_EXP("nothing at this point but");
        } else {
            return PS->s - src;
        }
        return 0;

      case IOP_JSON_EOF:
        if (!single_value)
            return 0;
        /* A union can't be empty */
        if (desc->is_union)
            return JERROR(IOP_JERR_NOTHING_TO_READ);

        /* At this point we check for any required field */
        for (int i = 0; i < desc->fields_len; i++) {
            if (__iop_skip_absent_field_desc(value, desc->fields + i) < 0)
                return RJERROR_SFIELD(IOP_JERR_MISSING_MEMBER,
                                      desc, desc->fields + i);
        }
        return 0;
      default:
        return RJERROR_EXP("`{'");
    }
}

/*-}}}-*/
/* {{{ jpack */

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

static int pack_txt(const iop_struct_t *desc, const void *value, int lvl,
                    int (*writecb)(void *, const void *buf, int len),
                    void *priv, bool strict)
{
    /* ints:   sign, 20 digits, and NUL -> 22
       double: sign, digit, dot, 17 digits, e, sign, up to 3 digits NUL -> 25
     */
    char ibuf[25];
    int res = 0;
    bool first = true;

    const iop_field_t *fdesc = desc->fields;
    const iop_field_t *fend  = desc->fields + desc->fields_len;

#define WRITE(b, l)    \
    do {                                                                   \
        int __res = do_write(writecb, priv, b, l);                         \
        if (__res < 0)                                                     \
            return -1;                                                     \
        res += __res;                                                      \
    } while (0)
#define PUTS(s)  WRITE(s, strlen(s))
#define INDENT() \
    do {                                                                   \
        int __res = do_indent(writecb, priv, lvl);                         \
        if (__res < 0)                                                     \
            return -1;                                                     \
        res += __res;                                                      \
    } while (0)
#define PUTU(i)  \
    WRITE(ibuf, sprintf(ibuf, "%llu", (unsigned long long)i))
#define PUTD(i)  \
    WRITE(ibuf, sprintf(ibuf, "%lld", (long long)i))

    if (desc->is_union) {
        fdesc = get_union_field(desc, value);
        fend  = fdesc + 1;
        PUTS("{ ");
    } else {
        PUTS("{\n");
        lvl++;
    }


    for (; fdesc < fend; fdesc++) {
        const void *ptr;
        bool repeated = fdesc->repeat == IOP_R_REPEATED;
        int n;

        ptr = get_n_and_ptr(fdesc, value, &n);
        if (!n && !repeated)
            continue;

        if (first) {
            first = false;
        } else {
            PUTS(",\n");
        }

        if (!desc->is_union)
            INDENT();
        PUTS("\"");
        PUTS(fdesc->name.s);
        PUTS(repeated ? "\": [ " : "\": ");

        for (int j = 0; j < n; j++) {
            if (repeated && j)
                PUTS(", ");
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
                v   = &IOP_FIELD(const char, ptr, j * fdesc->size);
                len = RETHROW(pack_txt(fdesc->u1.st_desc, v, lvl, writecb, priv, strict));
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
                if (IOP_FIELD(const iop_data_t, ptr, j).len) {
                    t_scope;
                    int dlen = IOP_FIELD(const iop_data_t, ptr, j).len;
                    int blen = 1 + DIV_ROUND_UP(dlen * 4, 3) + 1 + 1;
                    sb_t sb;

                    t_sb_init(&sb, blen);
                    sb_addc(&sb, '"');
                    sb_add_b64(&sb, IOP_FIELD(const iop_data_t, ptr, j).data,
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
            PUTS(" ]");
    }

    if (desc->is_union) {
        PUTS(" }");
    } else {
        if (!first)
            PUTS("\n");
        lvl--;
        INDENT();
        PUTS("}");
    }

    if (lvl == 0)
        PUTS("\n");

    return res;
#undef WRITE
#undef PUTS
#undef INDENT
#undef PUTU
#undef PUTI
}

int iop_jpack(const iop_struct_t *desc, const void *value,
              int (*writecb)(void *, const void *buf, int len),
              void *priv, bool strict)
{
    return pack_txt(desc, value, 0, writecb, priv, strict);
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
