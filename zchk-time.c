/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2011 INTERSEC SAS                                  */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "time.h"
#include "z.h"

Z_GROUP_EXPORT(time)
{
    Z_TEST(curday, "time: localtime_curday") {
        /* date -d "03/06/2007 12:34:13" +"%s" -> 1173180853 */
        /* date -d "03/06/2007 00:00:00" +"%s" -> 1173135600 */
        Z_ASSERT_EQ(localtime_curday(1173180853), 1173135600);

        /* The following test may fail if we are ***very*** unlucky, call
         * it the midnight bug!
         */
        Z_ASSERT_EQ(localtime_curday(0), localtime_curday(time(NULL)));
    } Z_TEST_END;

    Z_TEST(nextday, "time: localtime_nextday") {
        /* date -d "03/06/2007 12:34:13" +"%s" -> 1173180853 */
        /* date -d "03/07/2007 00:00:00" +"%s" -> 1173222000 */
        Z_ASSERT_EQ(localtime_nextday(1173180853), 1173222000);

        /* The following test may fail if we are ***very*** unlucky, call
         * it the midnight bug!
         */
        Z_ASSERT_EQ(localtime_nextday(0), localtime_nextday(time(NULL)));
    } Z_TEST_END;

    Z_TEST(strtom, "time: strtom") {
        struct tm t;

        p_clear(&t, 1);
        Z_ASSERT_N(strtotm("23-Jul-97", &t));
        Z_ASSERT_EQ(t.tm_mday, 23);
        Z_ASSERT_EQ(t.tm_mon + 1, 7);
        Z_ASSERT_EQ(t.tm_year + 1900, 1997);

        Z_ASSERT_NEG(strtotm("32-Jul-97", &t));
        Z_ASSERT_N(strtotm("29-Feb-96", &t));
        Z_ASSERT_N(strtotm("29-Feb-2000", &t));
        Z_ASSERT_N(strtotm("01-Jun-07", &t));
        Z_ASSERT_NEG(strtotm("31-Jun-07", &t));
    } Z_TEST_END;

    Z_TEST(iso8601_tz, "check that we grok timezone offsets properly")
    {
        pstream_t ps;
        time_t res;

        ps = ps_initstr("2007-03-06T11:34:13Z");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        ps = ps_initstr("2007-03-06T11:34:13+00:00");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        ps = ps_initstr("2007-03-06T11:34:13-00:00");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        ps = ps_initstr("2007-03-06T16:34:13+05:00");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        ps = ps_initstr("2007-03-07T01:34:13+14:00");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        ps = ps_initstr("2007-03-06T01:04:13-10:30");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        /* hours/minutes underflow */
        ps = ps_initstr("2007-03-07T00:04:13+12:30");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);

        /* hours/minutes overflow */
        ps = ps_initstr("2007-03-05T23:54:13-11:40");
        Z_ASSERT_N(time_parse_iso8601(&ps, &res));
        Z_ASSERT_EQ(res, 1173180853);
    } Z_TEST_END;
} Z_GROUP_END;
