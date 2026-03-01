// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>

#include "sum.h"

TEST(SumTests, GetHashEmpty)
{
    uint32_t h = get_hash("");
    EXPECT_EQ(h, 0u);
}

TEST(SumTests, GetHashSingleChar)
{
    uint32_t h = get_hash("a");
    EXPECT_EQ(h, (uint32_t)'a');
}

TEST(SumTests, GetHashKnownString)
{
    // Verify deterministic output
    uint32_t h1 = get_hash("hello");
    uint32_t h2 = get_hash("hello");
    EXPECT_EQ(h1, h2);
}

TEST(SumTests, GetHashDifferentStrings)
{
    uint32_t h1 = get_hash("hello");
    uint32_t h2 = get_hash("world");
    EXPECT_NE(h1, h2);
}

TEST(SumTests, GetHashCollisionResistance)
{
    // Spot-check that a few similar strings produce different hashes
    uint32_t h1 = get_hash("abc");
    uint32_t h2 = get_hash("abd");
    uint32_t h3 = get_hash("bbc");
    EXPECT_NE(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_NE(h2, h3);
}

TEST(SumTests, GetHashAlgorithm)
{
    // Verify the exact algorithm: ret = (ret * 4) ^ *p
    // For "ab":
    // step0: ret=0,
    // step1: ret=(0*4)^'a' = 97,
    // step2: ret=(97*4)^'b' = 388^98 = 486
    uint32_t h = get_hash("ab");
    EXPECT_EQ(h, 486);
}

TEST(SumTests, GetHashLongerString)
{
    // Test with a realistic participation filename
    uint32_t h = get_hash(".cfpart");
    EXPECT_NE(h, 0u);
}
