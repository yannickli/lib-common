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

#define IS_LIB_ISDB_INTERNAL
#define isdb_t void

#include <tchdb.h>

#include "mem.h"
#include "unix.h"
#include "db.h"

int db_error(isdb_t *db)
{
    return tchdbecode(db);
}

const char *db_strerror(isdb_t *db)
{
    return tchdberrmsg(tchdbecode(db));
}

isdb_t *db_open(const char *name, int flags, int oflags)
{
    int omode = (O_ISWRITE(flags) ? HDBOWRITER : HDBOREADER);
    TCHDB *db = tchdbnew();

    if (flags & O_CREAT)
        omode |= HDBOCREAT;
    if (flags & O_TRUNC)
        omode |= HDBOTRUNC;

    switch (oflags & (MMO_TLOCK | MMO_LOCK)) {
      case 0:
        omode |= HDBONOLCK;
        break;
      case MMO_TLOCK:
        omode |= HDBOLCKNB;
        break;
      default: /* at least MMO_LOCK */
        break;
    }
    if (tchdbopen(db, name, omode))
        return db;
    tchdbdel(db);
    return NULL;
}

int db_flush(isdb_t *db)
{
    return tchdbsync(db);
}

int db_close(isdb_t **_db)
{
    if (*_db) {
        int res = tchdbclose(*_db);
        *_db = NULL;
        return res;
    }
    return 0;
}

int db_get(isdb_t *db, const char *k, int kl, blob_t *out)
{
    int siz = tchdbvsiz(db, k, kl);
    int res;
    if (siz <= 0)
        return siz;

    blob_extend(out, siz);
    res = tchdbget3(db, k, kl, (char *)out->data + out->len - siz, siz + 1);
    if (res < 0) {
        blob_setlen(out, out->len - siz);
    }
    return res;
}

int db_put(isdb_t *db, const char *k, int kl, const char *v, int vl)
{
    return tchdbput(db, k, kl, v, vl);
}

int db_del(isdb_t *db, const char *k, int kl)
{
    return tchdbout(db, k, kl);
}
