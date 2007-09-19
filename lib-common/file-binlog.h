/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2007 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#ifndef IS_LIB_COMMON_FILE_BINLOG_H
#define IS_LIB_COMMON_FILE_BINLOG_H

#include "mem.h"

typedef struct binlog_t {
    flag_t writing : 1;
    flag_t dirty   : 1;
    uint32_t crc;
    off_t fsize;
    FILE *fp;
} binlog_t;

__must_check__ binlog_t *binlog_wopen(const char *name);
__must_check__ binlog_t *binlog_create(const char *name);
void binlog_close(binlog_t **);

int binlog_append(binlog_t *bl, uint16_t kind, const void *data, int len);
int binlog_flush(binlog_t *bl);

#endif
