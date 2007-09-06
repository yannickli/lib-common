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

#include <qdbm/depot.h>
#include <qdbm/curia.h>

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

isdb_t *db_open(const char *name, int mode, int flags, int bnum, int dnum)
{
    int omode = CR_OSPARSE | (O_ISWRITE(mode) ? CR_OWRITER : CR_OREADER);

    if (mode & O_CREAT)
        omode |= CR_OCREAT;
    if (mode & O_TRUNC)
        omode |= CR_OTRUNC;

    switch (flags & (MMO_TLOCK | MMO_LOCK)) {
      case 0:
        omode |= CR_ONOLCK;
        break;
      case MMO_TLOCK:
        omode |= CR_OLCKNB;
        break;
      default: /* at least MMO_LOCK */
        break;
    }

    return cropen(name, omode, bnum, dnum);
}

int db_flush(isdb_t *db)
{
    return crsync(db);
}

int db_close(isdb_t **_db)
{
    if (*_db) {
        int res = crclose(*_db);
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
        [DB_PUTOVER] = CR_DOVER,
        [DB_PUTKEEP] = CR_DKEEP,
        [DB_PUTMULT] = CR_DCAT,
    };
    return crput(db, k, kl, v, vl, conv[op]);
}

int db_del(isdb_t *db, const char *k, int kl)
{
    return dpout(db, k, kl);
}
