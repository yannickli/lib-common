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

#include <glob.h>

#include "file-log.h"
#include "z.h"

struct {
    int events[ LOG_FILE_DELETE + 1 ];
} log_files_calls_g;
#define _G log_files_calls_g


static void on_cb(struct log_file_t *file, enum log_file_event event,
                  const char *fpath, void *priv)
{
    _G.events[event]++;
}

Z_GROUP_EXPORT(file_log)
{
#define RANDOM_DATA_SIZE  (2 << 20)
#define NB_FILES          10

    Z_TEST(file_log_max_file_size, "check max_file_size") {
        t_scope;
        lstr_t      path = t_lstr_fmt("%*pMtmp_log",
                                      LSTR_FMT_ARG(z_tmpdir_g));
        char       *data = t_new_raw(char, RANDOM_DATA_SIZE);
        log_file_t *cfg;
        bool        waiting = true;

        Z_TEST_FLAGS("redmine_43539");

        /* read random stuff to avoid perfect compression */
        {
            int fd = open("/dev/urandom", O_RDONLY);
            Z_ASSERT_EQ(read(fd, data, RANDOM_DATA_SIZE), RANDOM_DATA_SIZE);
            close(fd);
        }

        /* create dummy log file */
        for(int i = 0; i < NB_FILES; i++) {
            lstr_t name = t_lstr_fmt("%s_19700101_%06d.log", path.s, i);
            file_t *file = file_open(name.s, FILE_WRONLY | FILE_CREATE, 0666);

            file_write(file, data, RANDOM_DATA_SIZE);
            IGNORE(file_close(&file));
        }

        cfg = log_file_new(path.s, LOG_FILE_COMPRESS);
        log_file_set_maxtotalsize(cfg, 1);
        log_file_set_file_cb(cfg, on_cb, NULL);

        Z_ASSERT_EQ(log_file_open(cfg, false), 0);
        Z_ASSERT_EQ(log_file_close(&cfg), 0);

        /* for each gz file, check if uncompressed file is still here */
        while (waiting) {
            glob_t globbuf;
            char buf[PATH_MAX];
            char **fv;
            int fc, ret;
            struct stat st;

            snprintf(buf, sizeof(buf), "%s_????????_??????.log.gz", path.s);
            ret = glob(buf, GLOB_BRACE, NULL, &globbuf);
            if (ret == GLOB_NOMATCH) {
                continue;
            }
            Z_ASSERT_EQ(ret, 0);

            fv = globbuf.gl_pathv;
            fc = globbuf.gl_pathc;
            waiting = false;
            for (int i = 0;  i < fc; i++) {
                fv[i][strlen(fv[i]) - 3] = '\0';
                if (stat(fv[i], &st) == 0) {
                    waiting = true;
                    break;
                }
            }
            globfree(&globbuf);
        }

        /* by calling log_file_open, we are sure that log_check_invariants
         * is called  */
        cfg = log_file_new(path.s, LOG_FILE_COMPRESS);
        log_file_set_maxtotalsize(cfg, 1);
        log_file_set_file_cb(cfg, on_cb, NULL);

        Z_ASSERT_EQ(log_file_open(cfg, false), 0);
        Z_ASSERT_EQ(log_file_close(&cfg), 0);

        /* last file may be reused */
        Z_ASSERT_GE(_G.events[LOG_FILE_DELETE], NB_FILES - 1);
    } Z_TEST_END;
#undef RANDOM_DATA_SIZE
#undef NB_FILES
} Z_GROUP_END
