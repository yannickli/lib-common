/**************************************************************************/
/*                                                                        */
/*  Copyright (C) 2004-2013 INTERSEC SA                                   */
/*                                                                        */
/*  Should you receive a copy of this source code, you must check you     */
/*  have a proper, written authorization of INTERSEC to hold it. If you   */
/*  don't have such an authorization, you must DELETE all source code     */
/*  files in your possession, and inform INTERSEC of the fact you obtain  */
/*  these files. Should you not comply to these terms, you can be         */
/*  prosecuted in the extent permitted by applicable law.                 */
/*                                                                        */
/**************************************************************************/

#include "datetime.h"
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
#define CHECK_DATE(str, res)  do {                                           \
            time_t ts;                                                       \
            Z_ASSERT_N(time_parse_iso8601s(str, &ts));                       \
            Z_ASSERT_EQ(ts, res);                                            \
        } while (0)

        CHECK_DATE("2007-03-06T11:34:13Z", 1173180853);
        CHECK_DATE("2007-03-06T11:34:13+00:00", 1173180853);
        CHECK_DATE("2007-03-06T11:34:13-00:00", 1173180853);
        CHECK_DATE("2007-03-06T16:34:13+05:00", 1173180853);
        CHECK_DATE("2007-03-07T01:34:13+14:00", 1173180853);
        CHECK_DATE("2007-03-06T01:04:13-10:30", 1173180853);

        /* hours/minutes underflow */
        CHECK_DATE("2007-03-07T00:04:13+12:30", 1173180853);

        /* hours/minutes overflow */
        CHECK_DATE("2007-03-05T23:54:13-11:40", 1173180853);
#undef CHECK_DATE
    } Z_TEST_END;

    Z_TEST(parse_tz, "check time parser")
    {
#define CHECK_DATE(str, res)  do {                                           \
            time_t ts;                                                       \
            Z_ASSERT_N(time_parse_str(str, &ts));                            \
            Z_ASSERT_EQ(ts, res);                                            \
        } while (0)

        /* ISO 8601 */
        CHECK_DATE("2007-03-06T11:34:13", 1173180853 + timezone);
        CHECK_DATE("2007-03-06T11:34:13Z", 1173180853);
        CHECK_DATE("2007-03-06t11:34:13z", 1173180853);
        CHECK_DATE("2007-03-06T11:34:13+00:00", 1173180853);
        CHECK_DATE("2007-03-06T11:34:13-00:00", 1173180853);
        CHECK_DATE("2007-03-06T16:34:13+05:00", 1173180853);
        CHECK_DATE("2007-03-07T01:34:13+14:00", 1173180853);
        CHECK_DATE("2007-03-06T01:04:13-10:30", 1173180853);

        /* hours/minutes underflow */
        CHECK_DATE("2007-03-07T00:04:13+12:30", 1173180853);

        /* hours/minutes overflow */
        CHECK_DATE("2007-03-05T23:54:13-11:40", 1173180853);

        /* RFC 822 */
        CHECK_DATE("6 Mar 2007 11:34:13", 1173180853 + timezone);
        CHECK_DATE("6 Mar 2007 11:34:13 GMT", 1173180853);
        CHECK_DATE("6 Mar 2007 11:34:13 +0000", 1173180853);
        CHECK_DATE("6 Mar 2007 11:34:13 -0000", 1173180853);
        CHECK_DATE("6 Mar 2007 16:34:13 +0500", 1173180853);
        CHECK_DATE("7 Mar 2007 01:34:13 +1400", 1173180853);
        CHECK_DATE("6 Mar 2007 01:04:13 -1030", 1173180853);

        /* hours/minutes underflow */
        CHECK_DATE("7 Mar 2007 00:04:13 +1230", 1173180853);

        /* hours/minutes overflow */
        CHECK_DATE("5 Mar 2007 23:54:13 -1140", 1173180853);


        CHECK_DATE("Tue, 6 Mar 2007 11:34:13", 1173180853 + timezone);
        CHECK_DATE("tUE, 6 MAr 2007 11:34:13", 1173180853 + timezone);
        CHECK_DATE("Tue, 6 Mar 2007 11:34:13 GMT", 1173180853);
        CHECK_DATE("Tue, 6 Mar 2007 11:34:13 +0000", 1173180853);
        CHECK_DATE("Tue, 6 Mar 2007 11:34:13 -0000", 1173180853);
        CHECK_DATE("Tue, 6 Mar 2007 16:34:13 +0500", 1173180853);
        CHECK_DATE("Wed, 7 Mar 2007 01:34:13 +1400", 1173180853);
        CHECK_DATE("Tue, 6 Mar 2007 01:04:13 -1030", 1173180853);

        /* hours/minutes underflow */
        CHECK_DATE("Wed, 7 Mar 2007 00:04:13 +1230", 1173180853);

        /* hours/minutes overflow */
        CHECK_DATE("Mon, 5 Mar 2007 23:54:13 -1140", 1173180853);

        /* Timestamp */
        CHECK_DATE("1173180853", 1173180853);
#undef CHECK_DATE
    } Z_TEST_END;

    Z_TEST(sb_add_localtime_iso8601, "time: sb_add_localtime_iso8601")
    {
        time_t ts = 1342088430; /* 2012-07-12T10:20:30Z */
        SB_1k(sb);

        sb_add_localtime_iso8601(&sb, ts, ":Indian/Antananarivo");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T13:20:30+03:00");

        sb.len = 0;
        sb_add_localtime_iso8601(&sb, ts, ":Asia/Katmandu");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T16:05:30+05:45");

        sb.len = 0;
        sb_add_localtime_iso8601(&sb, ts, ":America/Caracas");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T05:50:30-04:30");

        sb.len = 0;
        sb_add_localtime_iso8601(&sb, ts, ":Africa/Ouagadougou");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T10:20:30+00:00");
    } Z_TEST_END;

    Z_TEST(sb_add_localtime_iso8601_msec,
           "time: sb_add_localtime_iso8601_msec")
    {
        time_t ts = 1342088430; /* 2012-07-12T10:20:30Z */
        SB_1k(sb);

        sb_add_localtime_iso8601_msec(&sb, ts, 123, ":Indian/Antananarivo");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T13:20:30.123+03:00");

        sb.len = 0;
        sb_add_localtime_iso8601_msec(&sb, ts, 123, ":Asia/Katmandu");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T16:05:30.123+05:45");

        sb.len = 0;
        sb_add_localtime_iso8601_msec(&sb, ts, 123, ":America/Caracas");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T05:50:30.123-04:30");

        sb.len = 0;
        sb_add_localtime_iso8601_msec(&sb, ts, 123, ":Africa/Ouagadougou");
        Z_ASSERT_STREQUAL(sb.data, "2012-07-12T10:20:30.123+00:00");
    } Z_TEST_END;
} Z_GROUP_END;
