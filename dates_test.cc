// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <ctime>
#include <cstring>

#include "dates.h"

// Helper: get today at midnight as time_t
static time_t today_midnight()
{
    time_t t;
    time(&t);
    struct tm tm;
    memcpy(&tm, localtime(&t), sizeof(tm));
    tm.tm_sec = tm.tm_min = tm.tm_hour = 0;
    return mktime(&tm);
}

TEST(DatesTests, RelativePlus5)
{
    time_t tt = 0;
    time_t before_midnight = today_midnight();
    do_getdate(&tt, "+5");
    time_t after_midnight = today_midnight();
    time_t expected_before = before_midnight + 5 * 24 * 60 * 60;
    time_t expected_after = after_midnight + 5 * 24 * 60 * 60;
    EXPECT_TRUE(tt == expected_before || tt == expected_after);
}

TEST(DatesTests, RelativeMinus3)
{
    time_t tt = 0;
    time_t before_midnight = today_midnight();
    do_getdate(&tt, "-3");
    time_t after_midnight = today_midnight();
    time_t expected_before = before_midnight - 3 * 24 * 60 * 60;
    time_t expected_after = after_midnight - 3 * 24 * 60 * 60;
    EXPECT_TRUE(tt == expected_before || tt == expected_after);
}

TEST(DatesTests, RelativePlus0)
{
    time_t tt = 0;
    time_t before_midnight = today_midnight();
    do_getdate(&tt, "+0");
    time_t after_midnight = today_midnight();
    time_t expected_before = before_midnight;
    time_t expected_after = after_midnight;
    EXPECT_TRUE(tt == expected_before || tt == expected_after);
}

TEST(DatesTests, NamedMonthJan)
{
    time_t tt = 0;
    do_getdate(&tt, "jan 15");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 0);   // January
    EXPECT_EQ(tms->tm_mday, 15);
}

TEST(DatesTests, NamedMonthFebruary)
{
    time_t tt = 0;
    do_getdate(&tt, "february 28");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 1);   // February
    EXPECT_EQ(tms->tm_mday, 28);
}

TEST(DatesTests, NamedMonthAbbrevSep)
{
    time_t tt = 0;
    do_getdate(&tt, "sep 1");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 8);  // September
    EXPECT_EQ(tms->tm_mday, 1);
}

TEST(DatesTests, NumericSlashDate)
{
    time_t tt = 0;
    do_getdate(&tt, "1/15/2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 0);    // January
    EXPECT_EQ(tms->tm_mday, 15);
    EXPECT_EQ(tms->tm_year, 125); // 2025 - 1900
}

TEST(DatesTests, NumericDashDate)
{
    time_t tt = 0;
    do_getdate(&tt, "6-15-2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 5);    // June
    EXPECT_EQ(tms->tm_mday, 15);
    EXPECT_EQ(tms->tm_year, 125);
}

TEST(DatesTests, Year2000)
{
    time_t tt = 0;
    do_getdate(&tt, "1/1/2000");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_year, 100); // 2000 - 1900
}

TEST(DatesTests, TwoDigitYear)
{
    time_t tt = 0;
    do_getdate(&tt, "3/1/99");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_year, 99);  // 1999
}

TEST(DatesTests, WeekdayPrefixStripped)
{
    time_t tt = 0;
    do_getdate(&tt, "Mon, 6/15/2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 5);
    EXPECT_EQ(tms->tm_mday, 15);
    EXPECT_EQ(tms->tm_year, 125);
}

TEST(DatesTests, LeadingTimestamp)
{
    time_t tt = 0;
    do_getdate(&tt, "(14:30:00) 6/15/2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 5);
    EXPECT_EQ(tms->tm_mday, 15);
    EXPECT_EQ(tms->tm_hour, 14);
    EXPECT_EQ(tms->tm_min, 30);
    EXPECT_EQ(tms->tm_sec, 0);
}

TEST(DatesTests, TrailingTimestamp)
{
    time_t tt = 0;
    do_getdate(&tt, "6/15/2025 (10:05:30)");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_hour, 10);
    EXPECT_EQ(tms->tm_min, 5);
    EXPECT_EQ(tms->tm_sec, 30);
}

TEST(DatesTests, TrailingTimeNoParen)
{
    time_t tt = 0;
    do_getdate(&tt, "6/15/2025 10:05:30");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_hour, 10);
    EXPECT_EQ(tms->tm_min, 5);
}

TEST(DatesTests, AMModifier)
{
    time_t tt = 0;
    do_getdate(&tt, "(10:00 am) 6/15/2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_hour, 10);
}

TEST(DatesTests, PMModifier)
{
    time_t tt = 0;
    do_getdate(&tt, "(2:00 pm) 6/15/2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_hour, 14);
}

TEST(DatesTests, ReturnsPointerPastParsed)
{
    time_t tt = 0;
    const char *input = "+5 extra";
    const char *ptr = do_getdate(&tt, input);
    // ptr should point past the "+5" part
    EXPECT_GT(ptr, input);
    EXPECT_LE(ptr, input + strlen(input));
}

TEST(DatesTests, LeadingSpacesSkipped)
{
    time_t tt = 0;
    do_getdate(&tt, "   +0");
    EXPECT_EQ(tt, today_midnight());
}

TEST(DatesTests, MonthDayWithYear)
{
    time_t tt = 0;
    do_getdate(&tt, "dec 25 2025");
    struct tm *tms = localtime(&tt);
    EXPECT_EQ(tms->tm_mon, 11);   // December
    EXPECT_EQ(tms->tm_mday, 25);
    EXPECT_EQ(tms->tm_year, 125);
}
