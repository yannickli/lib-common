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

int strace_next_check;

void check_strace(void)
{
    char strace_status_buf[512];
    FILE *proc_status;
    char *p;
    int i;

    proc_status = fopen("/proc/self/status", "r");
    if (!proc_status) {
        fprintf(stderr, "Could not open /proc\n");
        exit(126);
    }
    if (!fread(strace_status_buf, 1, 512, proc_status)) {
        fprintf(stderr, "Could not read /proc\n");
        exit(125);
    }
    strace_status_buf[511] = '\0';
    p = strchr(strace_status_buf, '\n');/* Name */
    p++;
    p = strchr(p, '\n');/* State */
    p++;
    p = strchr(p, '\n');/* SleepAVG */
    p++;
    p = strchr(p, '\n');/* Tgid */
    p++;
    p = strchr(p, '\n');/* Pid */
    p++;
    p = strchr(p, '\n');/* PPid */
    p++;
    if (p[0] != 'T' || p[1] != 'r' || p[2] != 'a') {
        fprintf(stderr, "Bad /proc format (strace_status_buf = %s) (p = %s)\n",
                strace_status_buf, p);
        exit(124);
    }
    p = strchr(p, ':');
    p++;
    i = atoi(p);
    if (i) {
        /* Being traced !*/
        fprintf(stderr, "Bad constraint !\n");
        exit(124);
    }

    fclose(proc_status);
}

int find_extra_flags(byte *src, unsigned src_len,
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
    inbuf = p_new_raw(byte, insize);
    lseek(hd, 0L, SEEK_SET);
    if (read(hd, inbuf, insize) != insize) {
        if (flags) fprintf(stderr, "read\n");
        res = 3;
        goto done;
    }

    header = (unsigned *)(inbuf + 1880);        /* was 1748 */
    outsize = header[1];

    blockhdr = header + 3;
    size0 = blockhdr[0];
    size1 = blockhdr[1] & 0x7fffffff;

    if (blockhdr[1] & 0x80000000) {
        if (flags) fprintf(stderr, "flagged\n");
        goto done;
    }
    find_extra_flags((byte *)blockhdr + 8, size1,
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
    p_delete(&inbuf);
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

int find_extra_flags(byte *src, unsigned src_len,
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
