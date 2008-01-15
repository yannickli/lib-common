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
#include "cfg.h"

struct lexdata {
    int   context;
    char *cur_section;
    blob_t key, val, err;
};

static int parse_oct(const char *o) {
    int res = *o - '0';
    while (*++o) {
        res = (res * 10) + *o - '0';
    }
    return res;
}

static int parse_hex(const char *h) {
    return (str_digit_value(h[0]) << 4) + str_digit_value(h[1]);
}

/* there is no noyy* options for them */
__attribute__((unused)) static int yyget_column(yyscan_t yyscanner);
__attribute__((unused)) static void yyset_column(int column_no, yyscan_t yyscanner);

#define ERROR(fmt, ...)  \
    do {                                                                  \
        blob_set_fmt(&yyextra->err, "%d:"fmt, yyget_lineno(yyscanner),    \
                     ##__VA_ARGS__);                                      \
        return -1;                                                        \
    } while (0)

#define YY_EXTRA_TYPE  struct lexdata *
#define ECHO
%}

%option 8bit batch nointeractive reentrant stack yylineno
%option noyywrap nounput noyy_top_state noyy_push_state noyy_pop_state

%x      TOEOL
%x      SECTION EOSECTION
%x      EQUAL VALUE STRING

ID      [a-zA-Z][a-zA-Z0-9_]*
KEY     [a-zA-Z][a-zA-Z0-9_\-/.]*
HS      [ \t\r\v]
INT     "-"?[0-9]+

%%
<*>\\\n                     /* SKIP */

<INITIAL>{
    "["{HS}*                { p_delete(&yyextra->cur_section);
                              BEGIN(SECTION);
                            }
    {KEY}                   { if (!yyextra->cur_section)
                                  ERROR("no section opened yet");
                              blob_set_data(&yyextra->key, yytext, yyleng);
                              BEGIN(EQUAL);
                            }
}
<INITIAL,TOEOL>{
    {HS}+                   /* SKIP */
    [;#].*\n                { BEGIN(INITIAL); }
}

<SECTION>{
    {ID}{HS}*\"?            {
                              if (yytext[yyleng - 1] == '"') {
                                  yyextra->context = SECTION;
                                  blob_reset(&yyextra->val);
                                  yytext[--yyleng] = '\0';
                                  BEGIN(STRING);
                              } else {
                                  BEGIN(EOSECTION);
                              }
                              while (isspace((unsigned char)yytext[yyleng - 1]))
                                  yytext[--yyleng] = '\0';
                              yyextra->cur_section = p_dup(yytext, yyleng + 1);
                              printf("[%s]\n", yytext);
                            }
}
<EOSECTION>{HS}*"]"         { BEGIN(TOEOL); }

<EQUAL>{HS}*"="{HS}*        { BEGIN(VALUE); }
<VALUE>{
    {INT}                   { /* TODO do sth with strtoip(yytext, NULL); */;
                              printf("%s = %d\n", blob_get_cstr(&yyextra->key), strtoip(yytext, NULL));
                              BEGIN(TOEOL);
                            }
    \"                      { blob_reset(&yyextra->val);
                              yyextra->context = VALUE;
                              blob_reset(&yyextra->val);
                              BEGIN(STRING);
                            }
    [^[:space:];#]+         { /* TODO do sth with yytext */
                              printf("%s = %s\n", blob_get_cstr(&yyextra->key), yytext);
                              BEGIN(TOEOL);
                            }
}

<STRING>{
    \"                      {
                              switch (yyextra->context) {
                                case VALUE:
                                  /* ... */
                                  BEGIN(TOEOL);
                                  break;
                                case SECTION:
                                  /* ... */
                                  BEGIN(EOSECTION);
                                  break;
                                default: abort();
                              }
                            }
    \\[0-7]{1,3}            { blob_append_byte(&yyextra->val, parse_oct(yytext + 1)); }
    \\x[0-9a-fA-F]{2}       { blob_append_byte(&yyextra->val, parse_hex(yytext + 1)); }
    \\n                     { blob_append_byte(&yyextra->val, '\n'); }
    \\r                     { blob_append_byte(&yyextra->val, '\r'); }
    \\f                     { blob_append_byte(&yyextra->val, '\f'); }
    \\\\                    { blob_append_byte(&yyextra->val, '\\'); }
    [^\\\n\"]+              { blob_append_data(&yyextra->val, yytext, yyleng); }
}

<*>{
    \n                      { if (YYSTATE == INITIAL || YYSTATE == TOEOL) {
                                  BEGIN(INITIAL);
                              } else {
                                  ERROR("unexpected new line, missting `\\` ?");
                              }
                            }
    .                       { ERROR("unexpected char `%c`", *yytext); }
    <<EOF>>                 { if (YYSTATE == INITIAL || YYSTATE == TOEOL)
                                  return 0;
                              ERROR("unexpected end of file");
                            }
}

%%

int cfg_parse(const char *file)
{
    yyscan_t scanner;
    struct lexdata ld;
    FILE *fd = fopen(file, "rb");

    if (!fd)
        return -1;

    p_clear(&ld, 1);
    yylex_init(&scanner);
    blob_init(&ld.key);
    blob_init(&ld.val);
    blob_init(&ld.err);
    yyset_extra(&ld, scanner);
    yyset_in(fd, scanner);

    if (yylex(scanner) < 0)
        fprintf(stderr, "%s:%s\n", file, blob_get_cstr(&ld.err));

    yylex_destroy(scanner);
    blob_wipe(&ld.key);
    blob_wipe(&ld.val);
    blob_wipe(&ld.err);
    return 0;
}
