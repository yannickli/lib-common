/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2006 INTERSEC                                      */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_BLOB_PRIV_H
#define IS_LIB_COMMON_BLOB_PRIV_H
/*
 * A blob has a vital invariant, making every parse function avoid
 * buffer read overflows: there is *always* a '\0' in the data at
 * position len, implying that size is always >= len+1
 */
typedef struct {
    /* public interface */
    ssize_t len;
    byte *data;

    /* private interface */
    byte *area;   /* originally allocated block */
    ssize_t size;  /* allocated size */
    byte initial[BLOB_INITIAL_SIZE];
} real_blob_t;

#endif /* IS_LIB_COMMON_BLOB_PRIV_H */
