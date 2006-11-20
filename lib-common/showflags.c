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

/* FIXME: This module should not be compiled at all if EXPIRATION_DATE is
 * not defined. This should be done in the Makefile... */
/* OG: why is this a problem? */
#ifdef EXPIRATION_DATE

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "macros.h"

typedef unsigned int unsigned32;

int strace_last_check;
char strace_status_buf[512];

int nrv2b_flag_le32(byte *src, unsigned src_len,
                      unsigned *dst_len, unsigned flags);

int show_flags(const char *arg, int flags)
{
    int hd, insize, size0, size1, res = 0;
    byte *inbuf = NULL;
    unsigned outsize;
    unsigned block_outsize;
    unsigned *header;
    unsigned *blockhdr;
    struct stat st;

    if (stat(arg, &st)) {
        if (flags) fprintf(stderr, "stat\n");
        return 1;
    }
    if ((hd = open(arg, O_RDWR)) < 0) {
        if (flags) fprintf(stderr, "open\n");
        return 2;
    }
    insize = lseek(hd, 0L, SEEK_END);
    inbuf = malloc(insize);
    lseek(hd, 0L, SEEK_SET);
    if (read(hd, inbuf, insize) != insize) {
        if (flags) fprintf(stderr, "read\n");
        res = 3;
        goto done;
    }

    header = (unsigned *)(inbuf + 1748);
    outsize = header[1];

    blockhdr = header + 3;
    size0 = blockhdr[0];
    size1 = blockhdr[1] & 0x7fffffff;

    if (blockhdr[1] & 0x80000000) {
        if (flags) fprintf(stderr, "flagged\n");
        goto done;
    }
    nrv2b_flag_le32((byte *)blockhdr + 8, size1,
                    &block_outsize, st.st_ino);
    blockhdr[1] |= 0x80000000;

    if (block_outsize != outsize) {
        if (flags) fprintf(stderr, "size\n");
        res = 5;
        goto done;
    }
    lseek(hd, 0L, SEEK_SET);
    if (write(hd, inbuf, insize) != insize) {
        if (flags) fprintf(stderr, "write\n");
        res = 6;
        goto done;
    }
    if (flags) fprintf(stderr, "%s flagged\n", arg);
  done:
    free(inbuf);
    close(hd);
    return 0;
}

#define UA_GET4(src)       (*(const unsigned32*)(src))
#define UA_PUT4(src, val)  (*(unsigned32*)(src) = (val))

#define getbit_le32_flag(bb, bc, src, ilen, flags) \
    (bc > 0 ? ((bb>>--bc)&1) : (bc=31,\
              bb=UA_GET4((src)+ilen), \
              UA_PUT4((src)+ilen, bb^flags), \
              flags=(flags<<1)|(flags>>31) \
              ,ilen+=4,(bb>>31)&1))

#define getbit(bb)         getbit_le32_flag(bb,bc,src,ilen,flags)

int nrv2b_flag_le32(byte *src, unsigned src_len,
                    unsigned *dst_len, unsigned flags)
{
    unsigned bc = 0;
    unsigned32 bb = 0;
    unsigned ilen = 0, olen = 0, last_m_off = 1;

    for (;;) {
        unsigned m_off, m_len;

        while (getbit(bb)) {
            olen++;
            ilen++;
        }
        m_off = 1;
        do {
            m_off = m_off * 2 + getbit(bb);
        } while (!getbit(bb));

        if (m_off == 2) {
            m_off = last_m_off;
        } else {
            m_off = (m_off-3) * 256 + src[ilen++];
            if (m_off == 0xffffffff)
                break;
            last_m_off = ++m_off;
        }
        m_len = getbit(bb);
        m_len = m_len * 2 + getbit(bb);
        if (m_len == 0) {
            m_len++;
            do {
                m_len = m_len * 2 + getbit(bb);
            } while (!getbit(bb));
            m_len += 2;
        }
        m_len += (m_off > 0xd00);
        olen += m_len + 1;
    }
    *dst_len = olen;
    return ilen == src_len ? 0 : (ilen < src_len ? -1 : 1);
}

#endif
