/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2010 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "qlzo.h"

#define MODE_C  0
#define MODE_D  1

static int compress(const char *in, const char *out)
{
    byte buf[LZO_BUF_MEM_SIZE];
    sb_t sb, sbout;
    size_t sz;
    char path[PATH_MAX];

    sb_init(&sb);
    sb_init(&sbout);

    if (in) {
        RETHROW(sb_read_file(&sb, in));
    } else {
        int fd = RETHROW(open(in, O_RDONLY));

        while (RETHROW(sb_read(&sb, fd, 0)) > 0) {
        }
        close(fd);
    }

    sb_add(&sbout, &sb.len, 4);
    sz = lzo_cbuf_size(sb.len);
    sz = qlzo1x_compress(sb_growlen(&sbout, sz), sz,
                         ps_init(sb.data, sb.len), buf);
    __sb_fixlen(&sbout, 4 + sz);

    if (!out) {
        out = path;
        snprintf(path, sizeof(path), "%s.lzo", in ?: "out");
    }
    RETHROW(sb_write_file(&sbout, out));
    sb_wipe(&sb);
    sb_wipe(&sbout);
    return 0;
}

int main(int argc, char **argv)
{
    const char *out = NULL;
    int mode = MODE_C;
    int c;

    while ((c = getopt(argc, argv, "cdo:")) >= 0) {
        switch (c) {
          case 'c':
            mode = MODE_C;
            break;
          case 'd':
            mode = MODE_D;
            break;
          case 'o':
            out = optarg;
            break;
          default:
            fprintf(stderr, "error: unknown option '%c'", c);
            exit(1);
        }
    }
    if (argc - optind > 1) {
        fprintf(stderr, "error: too many arguments");
        exit(1);
    }

    if (mode == MODE_C) {
        return compress(argv[argc - 1], out);
    } else {
        //decompress(argv[argc - 1], out);
        return -1;
    }
}
