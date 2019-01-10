/**************************************************************************/
/*                                                                        */
/*  Copyright (C) INTERSEC SA                                             */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "licence.h"

/*
 * to use this file, #include it directly in the file that needs protection,
 * and call do_license_checks() from the point where you want to perform
 * checks.
 *
 * Note that this will significantly bloat your code, and is not a 0-cost
 * operation, do not put in in tight loops.
 *
 * See lib-common/el.c for example of use.
 *
 * Also: each time you include it, it adds a new constructor function that
 * will perform the checks first thing in the program's life.
 */

#define UA_GET4(src)       (*(const uint32_t*)(src))
#define UA_PUT4(src, val)  (*(uint32_t*)(src) = (val))

#define getbit_le32_flag(bb, bc, src, ilen, flags) \
    (bc > 0 ? ((bb>>--bc)&1) : (bc=31,\
              bb=UA_GET4((src)+ilen), \
              UA_PUT4((src)+ilen, bb^flags), \
              flags=(flags<<1)|(flags>>31) \
              ,ilen+=4,(bb>>31)&1))

#define getbit(bb)         getbit_le32_flag(bb,bc,src,ilen,flags)

static int find_extra_flags(byte *src, unsigned src_len,
                            unsigned *dst_len, unsigned flags)
{
    unsigned bc = 0;
    uint32_t bb = 0;
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

__attribute__((unused))
static int show_flags(const char *arg, int flags)
{
    int hd, insize, size1;
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
        goto done;
    }

#if 1
    if (*(unsigned*)(inbuf + 0x0078) == 0x5850557f) { /* \177UPX */
        header = (unsigned *)(inbuf + *(unsigned short*)(inbuf + 0x007C));
    } else {
        if (flags) fprintf(stderr, "unrecognized\n");
        goto done;
    }
#else
    header = (unsigned *)(inbuf + 1748);
    if (header[5] != 0x464c457f) {
        header = (unsigned *)(inbuf + 1880);
        if (header[5] != 0x464c457f) {
            header = (unsigned *)(inbuf + 1972);
            if (header[5] != 0x464c457f) {
                if (flags) fprintf(stderr, "unrecognized\n", arg);
                goto done;
            }
        }
    }
#endif

    outsize = header[1];

    blockhdr = header + 3;
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
        goto done;
    }
    lseek(hd, 0L, SEEK_SET);
    if (write(hd, inbuf, insize) != insize) {
        if (flags) fprintf(stderr, "write\n");
        goto done;
    }
    if (flags) fprintf(stderr, "%s flagged\n", arg);
  done:
    p_delete(&inbuf);
    close(hd);
    return 0;
}

#ifdef CHECK_TRACE

static ALWAYS_INLINE void check_strace(time_t now)
{
    static time_t next_strace_check;

    if (now < next_strace_check)
        return;

#define STRACE_CHECK_INTERVAL 2
    next_strace_check = now + STRACE_CHECK_INTERVAL;

    if (trace_override)
        return;

    if (_psinfo_get_tracer_pid(0) == 0)
        return;

    exit(124);
}
#endif

static ALWAYS_INLINE void do_license_checks(void)
{
#if defined(CHECK_TRACE) || defined(EXPIRATION_DATE)
    time_t now = (time)(NULL);

#ifdef EXPIRATION_DATE
    if (now > EXPIRATION_DATE) {
        fputs("Licence expired\n", stderr);
        exit(127);
    }
#endif
#ifdef CHECK_TRACE
    check_strace(now);
#endif
#endif
}

__attribute__((constructor))
static void license_checks_initialize(void)
{
    do_license_checks();
    //if (show_flags(__progname, 0)) {
    //    exit(42);
    //}
}
