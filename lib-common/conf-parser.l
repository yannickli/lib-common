%{
/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2008 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "strconv.h"
#include "conf.h"

struct lexdata {
    flag_t seen_section : 1;
    int   context;     /* state we just left, used by <STRING> state */
    int   lno, lstart; /* line no 1-based, position of the last \n   */
    int   pos, len;    /* stream length, 0-based, and copy of yyleng */

    cfg_parse_hook *hook;
    void *data;
    int   opts;
    char  path[PATH_MAX];

    FILE *fd;
    blob_t buf, err;
    yyscan_t scanner;
};

/* helpers {{{ */

static int lexdata_relpos(struct lexdata *ld, int offs)
{
    if (offs < 0) {
        assert (-offs <= ld->len);
        return ld->pos + ld->len + offs;
    } else {
        assert (offs < ld->len);
        return ld->pos + ld->len + offs;
    }
}

static void lexdata_newline(struct lexdata *ld, int offs)
{
    ld->lno++;
    ld->lstart = lexdata_relpos(ld, offs);
}

static int lexdata_col(struct lexdata *ld, int offs)
{
    return lexdata_relpos(ld, offs) - ld->lstart;
}

static char const __c_unescape[] = {
    ['a'] = '\a', ['b'] = '\b', ['e'] = '\e', ['t'] = '\t', ['n'] = '\n',
    ['v'] = '\v', ['f'] = '\f', ['r'] = '\r', ['\\'] = '\\',
};

static void blob_append_c_unescape(blob_t *blob, int c) {
    blob_append_byte(blob, __c_unescape[c]);
}
static int parse_oct(const char *o) {
    int res = *o - '0';
    while (*++o) {
        res = (res * 10) + *o - '0';
    }
    return res;
}

static int parse_hex(const char *h) {
    int res = str_digit_value(*h);
    while (*++h) {
        res = (res << 4) + str_digit_value(*h);
    }
    return res;
}

static int first_non_id_char(const char *txt)
{
    for (int pos = 0; ; pos++) {
        if (str_digit_value(txt[pos]) == 255) {
            if (txt[pos] != '_' || pos == 0)
                return pos;
        }
    }
}

/* }}} */
/* flex configuration {{{ */

/* there is no noyy* options for them */
__attribute__((unused)) static int yyget_column(yyscan_t yyscanner);
__attribute__((unused)) static void yyset_column(int column_no, yyscan_t yyscanner);

#define YY_USER_ACTION \
    yyextra->pos += yyextra->len;  \
    yyextra->len  = yyleng;

#define HOOK(evt, val, len) \
    do {                                                                    \
        if ((*yyextra->hook)(yyextra->data, (evt), (val), (len), yyextra))  \
            return -1;                                                      \
    } while (0)
#define HOOKBUF(evt)  HOOK((evt), blob_get_cstr(&yyextra->buf), yyextra->buf.len)

#define ERROR_OFFS(offs, fmt, ...)  \
    do {                                                                    \
        blob_set_fmt(&yyextra->err, "%s%d:%d:"fmt, yyextra->path,           \
                     yyextra->lno, lexdata_col(yyextra, (offs)),            \
                     ##__VA_ARGS__);                                        \
        HOOK(CFG_PARSE_ERROR, blob_get_cstr(&yyextra->err),                 \
             yyextra->err.len);                                             \
        return -1;                                                          \
    } while (0)

#define ERROR(fmt, ...)  ERROR_OFFS(0, fmt, ##__VA_ARGS__)
#define HAS_OPT(OPT)     (yyextra->opts & CFG_PARSE_##OPT)

#define YY_EXTRA_TYPE  struct lexdata *
#define ECHO

/* }}} */

%}

%option 8bit batch nointeractive reentrant stack
%option noyywrap nounput noyy_top_state noyy_push_state noyy_pop_state

%x      TOEOL
%x      SECTION EOSECTION
%x      EQUAL VALUE STRING

ID      [a-zA-Z][a-zA-Z0-9_]*
HS      [ \t\r\v]
INT     "-"?[0-9]+
ATOM    [^[:space:];#]+

%%
<*>\\\n                 { lexdata_newline(yyextra, -1); }

<INITIAL>{
    "["{HS}*            { BEGIN(SECTION); }
    {ID}(\.{ID})*       {
                          if (!yyextra->seen_section)
                              ERROR("no section opened yet");
                          HOOK(CFG_PARSE_KEY, yytext, yyleng);
                          BEGIN(EQUAL);
                        }
    {ID}(\.{ID})*\[\]   {
                          if (!yyextra->seen_section)
                              ERROR("no section opened yet");
                          if (HAS_OPT(GROK_ARRAY)) {
                              yytext[yyleng - 2] = '\0';
                              HOOK(CFG_PARSE_KEY_ARRAY, yytext, yyleng - 2);
                              BEGIN(EQUAL);
                          } else
                          if (HAS_OPT(OLD_KEYS)) {
                              HOOK(CFG_PARSE_KEY, yytext, yyleng);
                              BEGIN(EQUAL);
                          } else {
                              ERROR_OFFS(-2, "unexpected char `[`");
                          }
                        }
    [a-zA-Z]{ATOM}      {
                          if (!yyextra->seen_section)
                              ERROR("no section opened yet");
                          if (!HAS_OPT(OLD_KEYS)) {
                              int pos = first_non_id_char(yytext);
                              ERROR_OFFS(pos, "unexpected char `%c`", yytext[pos]);
                          }
                          HOOK(CFG_PARSE_KEY, yytext, yyleng);
                          BEGIN(EQUAL);
                        }
}
<INITIAL,TOEOL>{
    {HS}+               /* SKIP */
    [;#].*\n            { lexdata_newline(yyextra, -1); BEGIN(INITIAL); }
}

<SECTION>{
    {ID}{HS}*\"?        {
                          if (yytext[yyleng - 1] == '"') {
                              yyextra->context = SECTION;
                              yyleng--;
                              BEGIN(STRING);
                          } else {
                              BEGIN(EOSECTION);
                          }
                          while (isspace((unsigned char)yytext[yyleng - 1]))
                              yyleng--;
                          HOOK(CFG_PARSE_SECTION, yytext, yyleng);
                          yyextra->seen_section = true;
                        }
    {ID}:[^\n\]]+       {
                          if (!HAS_OPT(OLD_NAMESPACES)) {
                              ERROR_OFFS((int)(strchr(yytext, ':') - yytext),
                                         "unexpected char `:`");
                          }
                          while (isspace((unsigned char)yytext[yyleng - 1]))
                              yyleng--;
                          HOOK(CFG_PARSE_SECTION, yytext, yyleng);
                          yyextra->seen_section = true;
                          BEGIN(EOSECTION);
                        }
}
<EOSECTION>{HS}*"]"     { BEGIN(TOEOL); }

<EQUAL>{HS}*"="{HS}*    { BEGIN(VALUE); }

<VALUE>{ /* parse right hand side values */
    \n                  { HOOK(CFG_PARSE_VALUE, NULL, -1);
                          lexdata_newline(yyextra, -1);
                          BEGIN(INITIAL);
                        }
    {INT}               { HOOK(CFG_PARSE_VALUE, yytext, yyleng);
                          BEGIN(TOEOL);
                        }
    \"                  { yyextra->context = VALUE; BEGIN(STRING); }
    {ATOM}({HS}+{ATOM})* {
                          HOOK(CFG_PARSE_VALUE, yytext, yyleng);
                          BEGIN(TOEOL);
                        }
}


<STRING>{ /* parse string enclosed in double quotes with escapes */
    \"                  {
                          switch (yyextra->context) {
                            case VALUE:
                              HOOKBUF(CFG_PARSE_VALUE);
                              BEGIN(TOEOL);
                              break;
                            case SECTION:
                              HOOKBUF(CFG_PARSE_SECTION_ID);
                              BEGIN(EOSECTION);
                              break;
                            default: abort();
                          }
                          blob_reset(&yyextra->buf);
                        }
    \\[0-2][0-7]{2}     { blob_append_byte(&yyextra->buf, parse_oct(yytext + 1)); }
    \\x[0-9a-fA-F]{2}   { blob_append_byte(&yyextra->buf, parse_hex(yytext + 2)); }
    \\u[0-9a-fA-F]{4}   { blob_utf8_putc(&yyextra->buf, parse_hex(yytext + 2)); }
    \\[abetnvfr\\]      { blob_append_c_unescape(&yyextra->buf, yytext[1]); }
    \\.                 { blob_append_data(&yyextra->buf, yytext, 2); }
    [^\\\n\"]+          { blob_append_data(&yyextra->buf, yytext, yyleng); }
}

<*>{ /* default rules to catch end of lines and end of files */
    \n                  { if (YYSTATE != INITIAL && YYSTATE != TOEOL)
                              ERROR("unexpected new line, missing `\\` ?");
                          lexdata_newline(yyextra, 0);
                          BEGIN(INITIAL);
                        }
    .                   { ERROR("unexpected char `%c`", *yytext); }
    <<EOF>>             { if (YYSTATE != INITIAL && YYSTATE != TOEOL)
                              ERROR("unexpected end of file");
                          HOOK(CFG_PARSE_EOF, NULL, -1);
                          return 0;
                        }
}

%%

static struct lexdata *lexdata_fast_init(struct lexdata *ld)
{
    blob_init(&ld->buf);
    blob_init(&ld->err);
    ld->lno = 1;
    ld->lstart = -1;
    yylex_init(&ld->scanner);
    yyset_extra(ld, ld->scanner);
    return ld;
}

#define NOT_BOTH(x, o1, o2) \
    assert (!((x & CFG_PARSE_##o1) && (x & CFG_PARSE_##o2)))
static int check_opts(int opts)
{
    NOT_BOTH(opts, OLD_KEYS, GROK_ARRAY);
    return opts;
}

static void lexdata_wipe(struct lexdata *ld)
{
    blob_wipe(&ld->buf);
    blob_wipe(&ld->err);
    yylex_destroy(ld->scanner);
    p_fclose(&ld->fd);
}

int cfg_parse(const char *file, cfg_parse_hook *hook, void *data, int opts)
{
    struct lexdata ld = { .hook = hook, .data = data, .opts = check_opts(opts) };
    int res = -1;

    lexdata_fast_init(&ld);
    snprintf(ld.path, sizeof(ld.path), "%s:", file);
    if ((ld.fd = fopen(file, "rb"))) {
        yyset_in(ld.fd, ld.scanner);
        res = yylex(ld.scanner);
    }
    lexdata_wipe(&ld);
    return res;
}

int cfg_parse_buf(const char *buf, ssize_t blen, cfg_parse_hook *hook, void *data, int opts)
{
    struct lexdata ld = { .hook = hook, .data = data, .opts = check_opts(opts) };
    int res;

    lexdata_fast_init(&ld);
    yy_scan_bytes(buf, blen, ld.scanner);
    res = yylex(ld.scanner);
    lexdata_wipe(&ld);
    return res;
}

int cfg_parse_seterr(void *ctx, int offs, const char *fmt, ...)
{
    struct lexdata *ld = ctx;
    va_list ap;

    blob_set_fmt(&ld->err, "%s%d:%d:", ld->path, ld->lno, lexdata_col(ld, offs));
    va_start(ap, fmt);
    blob_append_vfmt(&ld->err, fmt, ap);
    va_end(ap);
    return -1;
}
