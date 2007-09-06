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

#define IS_LIB_ISDB_INTERNAL
#define isdb_t void

#include <fcntl.h>

#include <depot.h>

#include "mem.h"
#include "mmappedfile.h"
#include "unix.h"
#include "db.h"

int db_error(void)
{
    return dpecode;
}

const char *db_strerror(int code)
{
    return dperrmsg(code);
}

isdb_t *db_open(const char *name, int flags, int oflags, int bnum)
{
    int omode = DP_OSPARSE | (O_ISWRITE(flags) ? DP_OWRITER : DP_OREADER);

    if (flags & O_CREAT)
        omode |= DP_OCREAT;
    if (flags & O_TRUNC)
        omode |= DP_OTRUNC;

    switch (oflags & (MMO_TLOCK | MMO_LOCK)) {
      case 0:
        omode |= DP_ONOLCK;
        break;
      case MMO_TLOCK:
        omode |= DP_OLCKNB;
        break;
      default: /* at least MMO_LOCK */
        break;
    }

    return dpopen(name, omode, bnum);
}

int db_flush(isdb_t *db)
{
    return dpsync(db);
}

int db_close(isdb_t **_db)
{
    if (*_db) {
        int res = dpclose(*_db);
        *_db = NULL;
        return res;
    }
    return 0;
}

int db_get(isdb_t *db, const char *k, int kl, blob_t *out)
{
    int siz = dpvsiz(db, k, kl);
    int res;
    if (siz <= 0)
        return siz;

    blob_extend(out, siz);
    res = dpgetwb(db, k, kl, 0, siz, (char *)out->data + out->len - siz);
    if (res < 0) {
        blob_resize(out, out->len - siz);
    }
    return res;
}

int db_getbuf(isdb_t *db, const char *k, int kl, int start, int len, char *buf)
{
    return dpgetwb(db, k, kl, start, len, buf);
}

int db_put(isdb_t *db, const char *k, int kl, const char *v, int vl, int op)
{
    int conv[] = {
        [DB_PUTOVER] = DP_DOVER,
        [DB_PUTKEEP] = DP_DKEEP,
        [DB_PUTMULT] = DP_DCAT,
    };
    return dpput(db, k, kl, v, vl, conv[op]);
}

int db_del(isdb_t *db, const char *k, int kl)
{
    return dpout(db, k, kl);
}
