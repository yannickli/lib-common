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

#ifndef __MMAPPEDFILE_H__
#define __MMAPPEDFILE_H__

#include <sys/types.h>
#include <fcntl.h>

#include <lib-common/macros.h>

#define MMFILE_ALIAS(ptr_type) \
    {                              \
        int       fd;              \
        size_t    size;            \
        char     *path;            \
        ptr_type *area;            \
    }

typedef struct mmfile MMFILE_ALIAS(byte) mmfile;

mmfile *mmfile_open(const char *path, int flags);
mmfile *mmfile_creat(const char *path, off_t initialsize);
void mmfile_close(mmfile **mf);

/* @see ftruncate(2)
 *
 * XXX: mmfile may sometimes be wiped if remap fails !
 *      in that particular case, it returns -2 instead of -1
 */
int mmfile_truncate(mmfile *mf, off_t length);

#endif
