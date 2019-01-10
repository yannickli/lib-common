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

#include "datetime.h"
#include "z.h"

Z_GROUP_EXPORT(time)
{
    Z_TEST(curminute, "time: localtime_curminute") {
        /* date -d "03/06/2007 12:34:13" +"%s" -> 1173180853 */
        /* date -d "03/06/2007 12:34:00" +"%s" -> 1173180840 */
        Z_ASSERT_EQ(localtime_curminute(1173180853), 1173180840);
    } Z_TEST_END;

    Z_TEST(nextminute, "time: localtime_nextminute") {
        /* date -d "03/06/2007 12:34:13" +"%s" -> 1173180853 */
        /* date -d "03/06/2007 12:35:00" +"%s" -> 1173180900 */
        Z_ASSERT_EQ(localtime_nextminute(1173180853), 1173180900);

        /* date -d "03/06/2007 23:59:13" +"%s" -> 1173221953 */
        /* date -d "03/07/2007 00:00:00" +"%s" -> 1173222000 */
        Z_ASSERT_EQ(localtime_nextminute(1173221953), 1173222000);
    } Z_TEST_END;

    Z_TEST(curhour, "time: localtime_curhour") {
        /* date -d "03/06/2007 12:34:13" +"%s" -> 1173180853 */
        /* date -d "03/06/2007 12:00:00" +"%s" -> 1173178800 */
        Z_ASSERT_EQ(localtime_curhour(1173180853), 1173178800);
    } Z_TEST_END;

    Z_TEST(nexthour, "time: localtime_nexthour") {
        /* date -d "03/06/2007 12:34:13" +"%s" -> 1173180853 */
        /* date -d "03/06/2007 13:00:00" +"%s" -> 1173182400 */
        Z_ASSERT_EQ(localtime_nexthour(1173180853), 1173182400);

        /* date -d "03/06/2007 23:59:13" +"%s" -> 1173221953 */
        /* date -d "03/07/2007 00:00:00" +"%s" -> 1173222000 */
        Z_ASSERT_EQ(localtime_nexthour(1173221953), 1173222000);
    } Z_TEST_END;

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

    Z_TEST(curweek, "time: localtime_curweek") {
        /* Normal case */
        /* date -d '09/20/2013 14:31:33' +'%s' -> 1379680293 = friday */
        /* date -d "09/15/2013 00:00:00" +"%s" -> 1379196000 = sunday */
        /* date -d "09/16/2013 00:00:00" +"%s" -> 1379282400 = monday */
        Z_ASSERT_EQ(localtime_curweek(1379680293, 0), 1379196000);
        Z_ASSERT_EQ(localtime_curweek(1379680293, 1), 1379282400);

        /* wday < first_day_of_week */
        /* date -d '09/22/2013 14:31:33' +'%s' -> 1379853093 = sunday */
        /* date -d "09/22/2013 00:00:00" +"%s" -> 1379800800 = sunday */
        /* date -d "09/16/2013 00:00:00" +"%s" -> 1379282400 = monday */
        Z_ASSERT_EQ(localtime_curweek(1379853093, 0), 1379800800);
        Z_ASSERT_EQ(localtime_curweek(1379853093, 1), 1379282400);

        /* month/year transition */
        /* date -d '01/04/2014 14:38:21' +'%s' -> 1388842711 = saturday */
        /* date -d "12/29/2013 00:00:00" +"%s" -> 1388271600 = sunday */
        /* date -d "12/30/2013 00:00:00" +"%s" -> 1388358000 = monday */
        Z_ASSERT_EQ(localtime_curweek(1388842711, 0), 1388271600);
        Z_ASSERT_EQ(localtime_curweek(1388842711, 1), 1388358000);

        /* The following test may fail if we are ***very*** unlucky, call
         * it the midnight bug!
         */
        Z_ASSERT_EQ(localtime_curweek(0, 0),
                    localtime_curweek(time(NULL), 0));
    } Z_TEST_END;

    Z_TEST(nextweek, "time: localtime_nextweek") {
        /* Normal case */
        /* date -d '09/20/2013 14:31:33' +'%s' -> 1379680293 = friday */
        /* date -d "09/22/2013 00:00:00" +"%s" -> 1379800800 = sunday */
        /* date -d "09/23/2013 00:00:00" +"%s" -> 1379887200 = monday */
        Z_ASSERT_EQ(localtime_nextweek(1379680293, 0), 1379800800);
        Z_ASSERT_EQ(localtime_nextweek(1379680293, 1), 1379887200);

        /* wday < first_day_of_week */
        /* date -d '09/22/2013 14:31:33' +'%s' -> 1379853093 = sunday */
        /* date -d "09/29/2013 00:00:00" +"%s" -> 1380405600 = sunday */
        /* date -d "09/23/2013 00:00:00" +"%s" -> 1379887200 = monday */
        Z_ASSERT_EQ(localtime_nextweek(1379853093, 0), 1380405600);
        Z_ASSERT_EQ(localtime_nextweek(1379853093, 1), 1379887200);

        /* month/year transition */
        /* date -d '12/31/2013 14:38:21' +'%s' -> 1388497111 = tuesday */
        /* date -d "01/05/2014 00:00:00" +"%s" -> 1388876400 = sunday */
        /* date -d "01/06/2014 00:00:00" +"%s" -> 1388962800 = monday */
        Z_ASSERT_EQ(localtime_nextweek(1388497111, 0), 1388876400);
        Z_ASSERT_EQ(localtime_nextweek(1388497111, 1), 1388962800);

        /* The following test may fail if we are ***very*** unlucky, call
         * it the midnight bug!
         */
        Z_ASSERT_EQ(localtime_nextweek(0, 0),
                    localtime_nextweek(time(NULL), 0));
    } Z_TEST_END;

    Z_TEST(winter_time, "handle daylight saving CEST->CET 1382835600") {
        Z_ASSERT_EQ(localtime_curminute(1382835601), 1382835600);
        Z_ASSERT_EQ(localtime_nextminute(1382835599), 1382835600);

        Z_ASSERT_EQ(localtime_curhour(1382835601), 1382835600);
        Z_ASSERT_EQ(localtime_nexthour(1382835599), 1382835600);

        Z_ASSERT_EQ(localtime_curday(1382835601), 1382824800);
        Z_ASSERT_EQ(localtime_nextday(1382835599), 1382914800);

        Z_ASSERT_EQ(localtime_curweek(1382835601, 1), 1382306400);
        Z_ASSERT_EQ(localtime_nextweek(1382835599, 1), 1382914800);

        Z_ASSERT_EQ(localtime_curmonth(1382835601), 1380578400);
        Z_ASSERT_EQ(localtime_nextmonth(1382835599), 1383260400);
    } Z_TEST_END;

    Z_TEST(summer_time, "handle daylight saving CET->CEST 1364691600") {
        Z_ASSERT_EQ(localtime_curminute(1364691601), 1364691600);
        Z_ASSERT_EQ(localtime_nextminute(1364691599), 1364691600);

        Z_ASSERT_EQ(localtime_curhour(1364691601), 1364691600);
        Z_ASSERT_EQ(localtime_nexthour(1364691599), 1364691600);

        Z_ASSERT_EQ(localtime_curday(1364691601), 1364684400);
        Z_ASSERT_EQ(localtime_nextday(1364691599), 1364767200);

        Z_ASSERT_EQ(localtime_curweek(1364691601, 1), 1364166000);
        Z_ASSERT_EQ(localtime_nextweek(1364691599, 1), 1364767200);

        Z_ASSERT_EQ(localtime_curmonth(1364691601), 1362092400);
        Z_ASSERT_EQ(localtime_nextmonth(1364691599), 1364767200);
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

        /* Format of our syslogs */
        CHECK_DATE("2007-03-06 11:34:13 +0000", 1173180853);
        CHECK_DATE("2007-03-07 01:34:13 +1400", 1173180853);

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

    Z_TEST(iso8601_ms, "time: time_fmt_iso8601_msec")
    {
        char buf[1024];

        time_fmt_iso8601_msec(buf, 0, 999);
        Z_ASSERT_EQ(strlen(buf), 24U);
        time_fmt_iso8601_msec(buf, INT32_MAX, 0);
        Z_ASSERT_EQ(strlen(buf), 24U);
        time_fmt_iso8601_msec(buf, UINT32_MAX, 999);
        Z_ASSERT_EQ(strlen(buf), 24U);
    } Z_TEST_END;
} Z_GROUP_END;
