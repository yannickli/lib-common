/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2016 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "log.h"
#include "thr.h"
#include "z.h"

/* LCOV_EXCL_START */

static bool z_handler_was_used_g;

__attr_printf__(2, 0)
static void z_log_buffer_handler(const log_ctx_t *ctx, const char *fmt,
                                 va_list va)
{
    log_stderr_handler_g(ctx, fmt, va);
    z_handler_was_used_g = true;
}

typedef struct z_logger_init_job_t {
    thr_job_t job;

    logger_t  *loggers;
    thr_evc_t *ec;
    int        count;
} z_logger_init_job_t;

static void z_logger_init_job(thr_job_t *tjob, thr_syn_t *syn)
{
    z_logger_init_job_t *job = container_of(tjob, z_logger_init_job_t, job);

    for (int i = 0; i < job->count; i++) {
        logger_t *logger = &job->loggers[i];
        uint64_t key = thr_ec_get(job->ec);

        thr_ec_wait(job->ec, key);
        logger_notice(logger, "coucou");
    }
}

Z_GROUP_EXPORT(log) {
    Z_TEST(log_level, "log") {
        logger_t *root_logger = logger_get_by_name(LSTR_EMPTY_V);
        logger_t a = LOGGER_INIT_INHERITS(NULL, "a");
        logger_t b = LOGGER_INIT_SILENT_INHERITS(&a, "b");
        logger_t c = LOGGER_INIT(&b, "c", LOG_ERR);
        logger_t d;

        logger_init(&d, &c, LSTR("d"), LOG_INHERITS, LOG_SILENT);

        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&b));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_set_level(LSTR_EMPTY_V, LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(root_logger));

        logger_reset_level(LSTR_EMPTY_V);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&b));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_set_level(LSTR_EMPTY_V, LOG_WARNING, LOG_RECURSIVE);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(root_logger));

        logger_reset_level(LSTR_EMPTY_V);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&b));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_set_level(LSTR("a"), LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_set_level(LSTR("a"), LOG_WARNING, LOG_RECURSIVE);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_set_level(LSTR("a/b/c"), LOG_TRACE, 0);
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&c));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_reset_level(LSTR("a"));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_TRACE, logger_get_level(&c));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&b));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));

        logger_reset_level(LSTR("a/b/c"));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&d));
        Z_ASSERT_EQ(LOG_ERR, logger_get_level(&c));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&b));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&a));
        Z_ASSERT_EQ(root_logger->default_level,
                    logger_get_level(root_logger));


        /* Checks on silent flag */
        Z_ASSERT_EQ(a.level_flags, 0u);
        Z_ASSERT_EQ(b.level_flags, (unsigned)LOG_SILENT);
        Z_ASSERT_EQ(c.level_flags, 0u);
        Z_ASSERT_EQ(d.level_flags, (unsigned)LOG_SILENT);

        logger_set_level(LSTR("a/b"), LOG_WARNING, 0);
        Z_ASSERT_EQ(LOG_WARNING, logger_get_level(&b));
        Z_ASSERT_EQ(b.level_flags, 0u);
        logger_reset_level(LSTR("a/b"));
        Z_ASSERT_EQ(root_logger->default_level, logger_get_level(&b));
        Z_ASSERT_EQ(b.level_flags, (unsigned)LOG_SILENT);

        logger_wipe(&d);
        logger_wipe(&c);
        logger_wipe(&b);
        logger_wipe(&a);
    } Z_TEST_END;

    Z_TEST(get_loggers_confs, "get loggers confs") {
        t_scope;
        logger_t *root_logger = logger_get_by_name(LSTR_EMPTY_V);
        logger_t a = LOGGER_INIT_INHERITS(NULL, "ztest_log_conf");
        logger_t b = LOGGER_INIT_SILENT_INHERITS(&a, "son");
        logger_t c = LOGGER_INIT(&b, "gdson", LOG_ERR);
        logger_t d;
        qv_t(logger_conf) confs;

        logger_init(&d, &a, LSTR("sibl"), LOG_INHERITS, LOG_SILENT);

        /* force update of the tree */
        logger_get_level(&a);
        logger_get_level(&b);
        logger_get_level(&c);
        logger_get_level(&d);

        t_qv_init(logger_conf, &confs, 10);

        logger_get_all_configurations(LSTR("ztest_log_conf"), &confs);

        Z_ASSERT_EQ(confs.len, 4);
        Z_ASSERT_LSTREQUAL(confs.tab[0].full_name, LSTR("ztest_log_conf"));
        Z_ASSERT_LSTREQUAL(confs.tab[1].full_name,
                           LSTR("ztest_log_conf/son"));
        Z_ASSERT_LSTREQUAL(confs.tab[2].full_name,
                           LSTR("ztest_log_conf/son/gdson"));
        Z_ASSERT_LSTREQUAL(confs.tab[3].full_name,
                           LSTR("ztest_log_conf/sibl"));

        Z_ASSERT_EQ(confs.tab[0].level, root_logger->default_level);
        Z_ASSERT_EQ(confs.tab[1].level, root_logger->default_level);
        Z_ASSERT_EQ(confs.tab[2].level, LOG_ERR);
        Z_ASSERT_EQ(confs.tab[3].level, root_logger->default_level);

        Z_ASSERT_EQ(confs.tab[0].force_all, false);
        Z_ASSERT_EQ(confs.tab[1].force_all, false);
        Z_ASSERT_EQ(confs.tab[2].force_all, false);
        Z_ASSERT_EQ(confs.tab[3].force_all, false);

        Z_ASSERT_EQ(confs.tab[0].is_silent, false);
        Z_ASSERT_EQ(confs.tab[1].is_silent, true);
        Z_ASSERT_EQ(confs.tab[2].is_silent, false);
        Z_ASSERT_EQ(confs.tab[3].is_silent, true);

        logger_set_level(LSTR("ztest_log_conf"), LOG_WARNING, LOG_RECURSIVE);

        logger_get_all_configurations(LSTR("ztest_log_conf/son"),
                                      &confs);

        Z_ASSERT_EQ(confs.len, 6);
        Z_ASSERT_LSTREQUAL(confs.tab[4].full_name,
                           LSTR("ztest_log_conf/son"));
        Z_ASSERT_LSTREQUAL(confs.tab[5].full_name,
                           LSTR("ztest_log_conf/son/gdson"));

        Z_ASSERT_EQ(confs.tab[4].level, LOG_WARNING);
        Z_ASSERT_EQ(confs.tab[5].level, LOG_WARNING);

        Z_ASSERT_EQ(confs.tab[4].force_all, true);
        Z_ASSERT_EQ(confs.tab[5].force_all, true);

        logger_wipe(&d);
        logger_wipe(&c);
        logger_wipe(&b);
        logger_wipe(&a);
    } Z_TEST_END;

    Z_TEST(log_buffer, "log buffer") {
        log_handler_f *handler = log_set_handler(&z_log_buffer_handler);
        logger_t logger_test = LOGGER_INIT_SILENT(NULL, "logger test",
                                                  LOG_TRACE);
        logger_t logger_test_2 = LOGGER_INIT_SILENT(NULL, "logger test 2",
                                                  LOG_TRACE);
        const qv_t(log_buffer) *vect_buffer;

#define TEST_LOG_ENTRY(_i, _j, _level, _logger)                              \
    do {                                                                     \
        log_buffer_t entry = vect_buffer->tab[_i];                           \
        Z_ASSERT_LSTREQUAL(entry.msg,                                        \
                           LSTR("log message inside buffer " # _j));         \
        Z_ASSERT_LSTREQUAL(entry.ctx.logger_name, _logger.name);             \
        Z_ASSERT_EQ(entry.ctx.level, _level);                                \
    } while (0)

#define TEST_HANDLER(_state)                                                 \
    do {                                                                     \
        Z_ASSERT_EQ(z_handler_was_used_g, _state);                           \
        z_handler_was_used_g = false;                                        \
    } while (0)

        /* Buffer imbrication */

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test_2, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test_2, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);

        log_start_buffering(false);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        log_start_buffering(false);
        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 7);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 8, LOG_INFO, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(4, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 5, LOG_NOTICE, logger_test_2);
        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(1, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(3, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(4, 8, LOG_INFO, logger_test_2);

        logger_notice(&logger_test_2, "log message inside buffer %d", 5);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 3);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);

        log_start_buffering(false);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(1, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(3, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(4, 8, LOG_INFO, logger_test_2);

        /* buffer with handler */

        log_start_buffering(true);
        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");
        TEST_HANDLER(true);

        log_start_buffering(true);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        TEST_HANDLER(true);

        log_start_buffering(false);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);
        TEST_HANDLER(false);

        log_start_buffering(true);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);

        TEST_HANDLER(false);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        log_start_buffering(true);
        logger_notice(&logger_test, "log message inside buffer %d", 1);
        logger_error(&logger_test, "log message inside buffer %d", 2);
        logger_warning(&logger_test, "log message inside buffer %d", 3);
        logger_info(&logger_test, "log message inside buffer %d", 4);
        logger_notice(&logger_test_2, "log message inside buffer %d", 5);
        logger_error(&logger_test_2, "log message inside buffer %d", 6);
        logger_warning(&logger_test_2, "log message inside buffer %d", 7);
        logger_info(&logger_test_2, "log message inside buffer %d", 8);
        TEST_HANDLER(false);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        log_start_buffering(true);
        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");
        TEST_HANDLER(false);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 7);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 8);

        TEST_LOG_ENTRY(0, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(1, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(7, 8, LOG_INFO, logger_test_2);

        logger_info(&logger_test_2, "log message inside buffer %d", 8);
        TEST_HANDLER(true);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(1, 5, LOG_NOTICE, logger_test_2);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(3, 7, LOG_WARNING, logger_test_2);
        TEST_LOG_ENTRY(4, 8, LOG_INFO, logger_test_2);

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 7);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 8, LOG_INFO, logger_test_2);
        TEST_LOG_ENTRY(2, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(3, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(4, 4, LOG_INFO, logger_test);
        TEST_LOG_ENTRY(5, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(6, 5, LOG_NOTICE, logger_test_2);

        logger_notice(&logger_test_2, "log message inside buffer");
        log_start_buffering(true);
        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 0);

        /* buffer capture different level */

        log_start_buffering_filter(true, LOG_NOTICE);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 5);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(2, 1, LOG_NOTICE, logger_test);
        TEST_LOG_ENTRY(3, 6, LOG_ERR, logger_test_2);
        TEST_LOG_ENTRY(4, 5, LOG_NOTICE, logger_test_2);

        log_start_buffering_filter(true, LOG_WARNING);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");

        log_start_buffering_filter(true, LOG_ERR);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 2);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 6, LOG_ERR, logger_test_2);

        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 3);

        TEST_LOG_ENTRY(0, 2, LOG_ERR, logger_test);
        TEST_LOG_ENTRY(1, 3, LOG_WARNING, logger_test);
        TEST_LOG_ENTRY(2, 6, LOG_ERR, logger_test_2);

        log_start_buffering_filter(true, LOG_ALERT);

        logger_error(&logger_test, "log message inside buffer 2");
        logger_info(&logger_test_2, "log message inside buffer 8");
        logger_warning(&logger_test, "log message inside buffer 3");
        logger_notice(&logger_test, "log message inside buffer 1");
        logger_info(&logger_test, "log message inside buffer 4");
        logger_error(&logger_test_2, "log message inside buffer 6");
        logger_notice(&logger_test_2, "log message inside buffer 5");

        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 0);

        logger_notice(&logger_test_2, "log message inside buffer");
        log_start_buffering_filter(true, LOG_TRACE);
        vect_buffer = log_stop_buffering();
        Z_ASSERT_EQ(vect_buffer->len, 0);

        logger_wipe(&logger_test);
        logger_wipe(&logger_test_2);

        log_set_handler(handler);
#undef TEST_LOG_ENTRY
    } Z_TEST_END;

    Z_TEST(scope, "scoped logs") {
        logger_t l = LOGGER_INIT_INHERITS(NULL, "blah");

#define _CHECK_RES(Level, Catch)  do {                                       \
        if (Catch) {                                                         \
            Z_ASSERT_EQ(vect_buffer->len, 2);                                \
            Z_ASSERT_EQ(vect_buffer->tab[0].ctx.level, Level);               \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[0].msg, LSTR("coucou2"));    \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[0].ctx.logger_name,          \
                               LSTR("blah"));                                \
            Z_ASSERT_EQ(vect_buffer->tab[1].ctx.level, Level);               \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[1].msg, LSTR("coucou"));     \
            Z_ASSERT_LSTREQUAL(vect_buffer->tab[1].ctx.logger_name,          \
                               LSTR("blah"));                                \
        } else {                                                             \
            Z_ASSERT_ZERO(vect_buffer->len);                                 \
        }                                                                    \
    } while (0)

#define TEST(Scope, Start, End, Level, Catch)  do {                          \
        const qv_t(log_buffer) *vect_buffer;                                 \
                                                                             \
        log_start_buffering(false);                                          \
        Start;                                                               \
        logger_cont("coucou");                                               \
        logger_cont("%d", 2);                                                \
        logger_cont("\ncou");                                                \
        logger_cont("cou");                                                  \
        End;                                                                 \
        vect_buffer = log_stop_buffering();                                  \
        _CHECK_RES(Level, Catch);                                            \
                                                                             \
        log_start_buffering(false);                                          \
        {                                                                    \
            Scope;                                                           \
            logger_cont("coucou");                                           \
            logger_cont("%d", 2);                                            \
            logger_cont("\ncou");                                            \
            logger_cont("cou");                                              \
        }                                                                    \
        vect_buffer = log_stop_buffering();                                  \
        _CHECK_RES(Level, Catch);                                            \
    } while (0)

        TEST(logger_error_scope(&l),
             logger_error_start(&l), logger_end(&l),
             LOG_ERR, true);
        TEST(logger_warning_scope(&l),
             logger_warning_start(&l), logger_end(&l),
             LOG_WARNING, true);
        TEST(logger_notice_scope(&l),
             logger_notice_start(&l), logger_end(&l),
             LOG_NOTICE, true);
        TEST(logger_info_scope(&l),
             logger_info_start(&l), logger_end(&l),
             LOG_INFO, true);
        TEST(logger_debug_scope(&l),
             logger_debug_start(&l), logger_end(&l),
             LOG_DEBUG, true);
        TEST(logger_trace_scope(&l, 0),
             logger_trace_start(&l, 0), logger_end(&l),
             LOG_TRACE, true);
        TEST(logger_trace_scope(&l, 3),
             logger_trace_start(&l, 3), logger_end(&l),
             LOG_TRACE + 3, false);

        logger_set_level(LSTR("blah"), LOG_TRACE + 8, 0);
        TEST(logger_trace_scope(&l, 3),
             logger_trace_start(&l, 3), logger_end(&l),
             LOG_TRACE + 3, true);
#undef TEST

        logger_wipe(&l);
    } Z_TEST_END;

    Z_TEST(thr, "concurrent access to a logger") {
        t_scope;
        thr_syn_t syn;
        thr_evc_t ec;
        logger_t parent0 = LOGGER_INIT_INHERITS(NULL, "thrbase");
        logger_t parent1[2] = {
            LOGGER_INIT_INHERITS(&parent0, "a"),
            LOGGER_INIT_INHERITS(&parent0, "b")
        };
        logger_t parent2[4] = {
            LOGGER_INIT_INHERITS(&parent1[0], "a"),
            LOGGER_INIT_INHERITS(&parent1[0], "b"),
            LOGGER_INIT_INHERITS(&parent1[1], "c"),
            LOGGER_INIT_INHERITS(&parent1[1], "d")
        };
        logger_t children[10] = {
            LOGGER_INIT_INHERITS(&parent2[0], "a"),
            LOGGER_INIT_INHERITS(&parent2[0], "b"),
            LOGGER_INIT_INHERITS(&parent2[1], "c"),
            LOGGER_INIT_INHERITS(&parent2[1], "d"),
            LOGGER_INIT_INHERITS(&parent2[2], "e"),
            LOGGER_INIT_INHERITS(&parent2[2], "f"),
            LOGGER_INIT_INHERITS(&parent2[3], "g"),
            LOGGER_INIT_INHERITS(&parent2[3], "h"),
            LOGGER_INIT_INHERITS(&parent2[3], "i"),
            LOGGER_INIT_INHERITS(&parent2[3], "j")
        };

        MODULE_REQUIRE(thr);

        thr_syn_init(&syn);
        thr_ec_init(&ec);

        for (size_t i = 0; i < thr_parallelism_g - 1; i++) {
            z_logger_init_job_t *job = t_new(z_logger_init_job_t, 1);

            job->job.run = &z_logger_init_job;
            job->count = countof(children);
            job->loggers = children;
            job->ec      = &ec;

            thr_syn_schedule(&syn, &job->job);
        }

        for (int c = 0; c < countof(children); c++) {
            sleep(1);
            thr_ec_broadcast(&ec);
            logger_notice(&children[c], "coucou");
        }

        Z_ASSERT(true);

        thr_syn_wait(&syn);
        thr_syn_wipe(&syn);
        thr_ec_wipe(&ec);

        MODULE_RELEASE(thr);
    } Z_TEST_END;

    Z_TEST(parse_specs, "test parsing of IS_DEBUG environment variable") {
        t_scope;
        qv_t(spec) specs;

        t_qv_init(spec, &specs, 8);

#define TEST(_str, _nb)  \
        do {                                                                 \
            qv_clear(spec, &specs);                                          \
            log_parse_specs(t_strdup(_str), &specs);                         \
            Z_ASSERT_EQ(specs.len, _nb);                                     \
        } while (0)

        TEST("   ", 0);

        TEST("log.c", 1);
        Z_ASSERT_STREQUAL(specs.tab[0].path, "log.c");
        Z_ASSERT_NULL(specs.tab[0].func);
        Z_ASSERT_NULL(specs.tab[0].name);
        Z_ASSERT_EQ(specs.tab[0].level, INT_MAX);

        TEST("@log_parse_specs", 1);
        Z_ASSERT_NULL(specs.tab[0].path);
        Z_ASSERT_STREQUAL(specs.tab[0].func, "log_parse_specs");
        Z_ASSERT_NULL(specs.tab[0].name);
        Z_ASSERT_EQ(specs.tab[0].level, INT_MAX);

        TEST("+log/tracing", 1);
        Z_ASSERT_NULL(specs.tab[0].path);
        Z_ASSERT_NULL(specs.tab[0].func);
        Z_ASSERT_STREQUAL(specs.tab[0].name, "log/tracing");
        Z_ASSERT_EQ(specs.tab[0].level, INT_MAX);

        TEST(":5", 1);
        Z_ASSERT_NULL(specs.tab[0].path);
        Z_ASSERT_NULL(specs.tab[0].func);
        Z_ASSERT_NULL(specs.tab[0].name);
        Z_ASSERT_EQ(specs.tab[0].level, LOG_TRACE + 5);

        TEST(" log.c@log_parse_specs+log/tracing:5  ", 1);
        Z_ASSERT_STREQUAL(specs.tab[0].path, "log.c");
        Z_ASSERT_STREQUAL(specs.tab[0].func, "log_parse_specs");
        Z_ASSERT_STREQUAL(specs.tab[0].name, "log/tracing");
        Z_ASSERT_EQ(specs.tab[0].level, LOG_TRACE + 5);

        TEST(" log.c@log_parse_specs+log/tracing:5  log.c", 2);
        Z_ASSERT_STREQUAL(specs.tab[0].path, "log.c");
        Z_ASSERT_STREQUAL(specs.tab[0].func, "log_parse_specs");
        Z_ASSERT_STREQUAL(specs.tab[0].name, "log/tracing");
        Z_ASSERT_EQ(specs.tab[0].level, LOG_TRACE + 5);
        Z_ASSERT_STREQUAL(specs.tab[1].path, "log.c");
        Z_ASSERT_NULL(specs.tab[1].func);
        Z_ASSERT_NULL(specs.tab[1].name);
        Z_ASSERT_EQ(specs.tab[1].level, INT_MAX);

#undef TEST
    } Z_TEST_END;

    Z_TEST(sanitize_names, "check name sanitizer") {
        t_scope;

#define TEST(_str, _nstr)                                                    \
        do {                                                                 \
            lstr_t _orig = t_lstr_fmt(_str);                                 \
            lstr_t _dest = t_lstr_fmt(_nstr);                                \
            Z_ASSERT_LSTREQUAL(_dest, t_logger_sanitize_name(_orig));        \
        } while (0)

#define TEST_NULL(_str)                                                      \
        do {                                                                 \
            lstr_t _orig = t_lstr_fmt(_str);                                 \
            Z_ASSERT_LSTREQUAL(LSTR_EMPTY_V, t_logger_sanitize_name(_orig)); \
        } while (0)

        TEST("remove all spaces", "removeallspaces");
        TEST("remove/this!", "removethis");
        TEST("!!/ //4!/ !", "4");
        TEST(" A} ", "A}");
        TEST_NULL("!!/");
        TEST_NULL("");
        TEST_NULL(" !/ ");
        TEST_NULL("} toto");
#undef TEST
#undef TEST_NULL

    } Z_TEST_END;

} Z_GROUP_END;

/* LCOV_EXCL_STOP */
