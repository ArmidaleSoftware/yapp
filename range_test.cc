// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cstring>
#include <ctime>

#include "globals.h"
#include "range.h"
#include "struct.h"
#include "yapp.h"

// --- is_newresp ---

TEST(RangeTests, IsNewrespNoParticipation)
{
    partentry_t p = {0, 0};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    EXPECT_EQ(is_newresp(&p, &s), 0);
}

TEST(RangeTests, IsNewrespHasNewResponses)
{
    partentry_t p = {3, 900};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    flags = 0;
    EXPECT_EQ(is_newresp(&p, &s), 1);
}

TEST(RangeTests, IsNewrespNoNewTimestamp)
{
    partentry_t p = {5, 1000};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    flags = 0;
    EXPECT_EQ(is_newresp(&p, &s), 0);
}

TEST(RangeTests, IsNewrespSensitiveTimestampOnly)
{
    // With O_SENSITIVE, a timestamp change alone is enough
    partentry_t p = {5, 900};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    flags = O_SENSITIVE;
    EXPECT_EQ(is_newresp(&p, &s), 1);
}

TEST(RangeTests, IsNewrespSensitiveNoTimestampChange)
{
    partentry_t p = {5, 1000};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    flags = O_SENSITIVE;
    EXPECT_EQ(is_newresp(&p, &s), 0);
}

TEST(RangeTests, IsNewrespNegativeNr)
{
    // Negative nr means forgotten, abs(nr) is used
    partentry_t p = {-3, 900};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    flags = 0;
    EXPECT_EQ(is_newresp(&p, &s), 1);
}

// --- is_brandnew ---

TEST(RangeTests, IsBrandnewYes)
{
    partentry_t p = {0, 0};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    EXPECT_EQ(is_brandnew(&p, &s), 1);
}

TEST(RangeTests, IsBrandnewAlreadySeen)
{
    partentry_t p = {3, 900};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    EXPECT_EQ(is_brandnew(&p, &s), 0);
}

TEST(RangeTests, IsBrandnewNoResponses)
{
    partentry_t p = {0, 0};
    sumentry_t s = {IF_ACTIVE, 0, 1000, 500};
    EXPECT_EQ(is_brandnew(&p, &s), 0);
}

TEST(RangeTests, IsBrandnewTimestampNotNewer)
{
    partentry_t p = {0, 2000};
    sumentry_t s = {IF_ACTIVE, 5, 1000, 500};
    EXPECT_EQ(is_brandnew(&p, &s), 0);
}

// --- get_number ---

TEST(RangeTests, GetNumberFirst)
{
    status_t st = {};
    st.i_first = 1;
    st.i_last = 100;
    st.i_current = 50;
    EXPECT_EQ(get_number("first", &st), 1);
    EXPECT_EQ(get_number("fi", &st), 1);
    EXPECT_EQ(get_number("^", &st), 1);
}

TEST(RangeTests, GetNumberLast)
{
    status_t st = {};
    st.i_first = 1;
    st.i_last = 100;
    st.i_current = 50;
    EXPECT_EQ(get_number("last", &st), 100);
    EXPECT_EQ(get_number("l", &st), 100);
    EXPECT_EQ(get_number("$", &st), 100);
}

TEST(RangeTests, GetNumberThis)
{
    status_t st = {};
    st.i_first = 1;
    st.i_last = 100;
    st.i_current = 50;
    EXPECT_EQ(get_number("this", &st), 50);
    EXPECT_EQ(get_number("current", &st), 50);
    EXPECT_EQ(get_number(".", &st), 50);
}

TEST(RangeTests, GetNumberPlainNumber)
{
    status_t st = {};
    st.i_first = 1;
    st.i_last = 100;
    EXPECT_EQ(get_number("42", &st), 42);
}

TEST(RangeTests, GetNumberDotFormat)
{
    status_t st = {};
    st.i_first = 1;
    st.i_last = 100;
    // For "5.3" format, sum[4].nr must be > 3
    sum[4].nr = 10;
    EXPECT_EQ(get_number("5.3", &st), 5);
    EXPECT_EQ(st.r_first, 3);
    sum[4].nr = 0;  // cleanup
}

TEST(RangeTests, GetNumberZero)
{
    status_t st = {};
    EXPECT_EQ(get_number("0", &st), 0);
}

// --- rangeinit ---

TEST(RangeTests, RangeinitClearsStatus)
{
    status_t st = {};
    char act[MAX_ITEMS];
    st.string = "test";
    st.author = "someone";
    st.since = 12345;
    st.before = 67890;
    st.opt_flags = 0xFF;
    st.rng_flags = 0xFF;
    memset(act, A_FORCE, sizeof(act));

    rangeinit(&st, act);

    EXPECT_TRUE(st.string.empty());
    EXPECT_TRUE(st.author.empty());
    EXPECT_EQ(st.since, 0);
    EXPECT_EQ(st.before, 0);
    EXPECT_EQ(st.opt_flags, 0);
    EXPECT_EQ(st.rng_flags, 0);
    for (int i = 0; i < MAX_ITEMS; i++)
        EXPECT_EQ(act[i], 0);
}
