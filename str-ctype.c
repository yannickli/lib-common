/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2012 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "str.h"

/* ctype description for tokens "abcdefghijklmnopqrstuvwxyz" */
ctype_desc_t const ctype_islower = {
    {
        0x00000000,
        0x00000000,
        0x00000000,
        0x07fffffe,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    }
};

/* ctype description for tokens "ABCDEFGHIJKLMNOPQRSTUVWXYZ" */
ctype_desc_t const ctype_isupper = {
    {
        0x00000000,
        0x00000000,
        0x07fffffe,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    }
};

/* ctype description for tokens "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" */
ctype_desc_t const ctype_isalnum = {
    {
        0x00000000,
        0x03ff0000,
        0x07fffffe,
        0x07fffffe,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    }
};

/* ctype description for tokens "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ" */
ctype_desc_t const ctype_isalpha = {
    {
        0x00000000,
        0x00000000,
        0x07fffffe,
        0x07fffffe,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    }
};

/* ctype description for tokens "0123456789" */
ctype_desc_t const ctype_isdigit = {
    {
        0x00000000,
        0x03ff0000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    }
};

/* ctype description for tokens "0123456789abcdefABCDEF" */
ctype_desc_t const ctype_ishexdigit = {
    {
        0x00000000, 0x03ff0000, 0x0000007e, 0x0000007e,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
    }
};

/* XXX Valid C variable name should match this span *and* should not begin
 *     with a numeric character
 */
/* ctype description for tokens "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_" */
ctype_desc_t const ctype_iscvar = {
    {
        0x00000000,
        0x03ff0000,
        0x87fffffe,
        0x07fffffe,
        0x00000000,
        0x00000000,
        0x00000000,
        0x00000000,
    }
};


/* ctype description for tokens "01" */
ctype_desc_t const ctype_isbindigit = {
    {
        0x00000000, 0x00030000, 0x00000000, 0x00000000,
        0x00000000, 0x00000000, 0x00000000, 0x00000000,
    }
};

/* ctype description for s:" \f\n\r\t\v" */
ctype_desc_t const ctype_isspace = { {
    0x00003e00, 0x00000001, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
} };


/* ctype describing characters of a word */
ctype_desc_t const ctype_iswordpart = {
    {
        0x00000000,
        0x03ff0010,
        0x07ffffff,
        0x07fffffe,
        0xffffffff,
        0xffffffff,
        0xffffffff,
        0xffffffff,
    }
};
