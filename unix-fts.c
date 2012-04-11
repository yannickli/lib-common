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

/* YES I'm sure: we don't use the ->stat structure in our fts ! */
#undef _LARGEFILE64_SOURCE
#undef _FILE_OFFSET_BITS
#include <fts.h>
#include "unix.h"

int rmdir_r(const char *dir, bool only_content)
{
    char * const argv[2] = { (char *)dir, NULL };
    FTS *fts;
    FTSENT *ent;
    int res = 0;

    fts = fts_open(argv, FTS_NOSTAT | FTS_PHYSICAL | FTS_NOCHDIR, NULL);
    if (!fts)
        return -1;

    while ((ent = fts_read(fts)) != NULL) {
        if (ent->fts_level <= 0)
            continue;
        switch (ent->fts_info) {
          case FTS_D:
            break;
          case FTS_DC:
          case FTS_DNR:
          case FTS_ERR:
            res = -1;
            break;
          case FTS_DP:
            if (rmdir(ent->fts_accpath))
                res = -1;
            break;
          default:
            if (unlink(ent->fts_accpath))
                res = -1;
            break;
        }
    }
    fts_close(fts);

    if (!only_content && rmdir(dir))
        res = -1;
    return res;
}
