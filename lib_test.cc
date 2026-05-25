// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "lib.h"
#include "str.h"
#include "struct.h"

// --- strim / strimw ---

TEST(LibTests, StrimDefault)
{
    EXPECT_EQ(strim("  hello  "), "hello");
    EXPECT_EQ(strim("hi there "), "hi there");
}

TEST(LibTests, StrimwWhitespace)
{
    EXPECT_EQ(strimw(" \t hello \n "), "hello");
}

TEST(LibTests, StrimEmpty)
{
    EXPECT_EQ(strim(""), "");
}

// --- trim(char*) ---

TEST(LibTests, TrimNull)
{
    EXPECT_EQ(trim(nullptr), nullptr);
}

TEST(LibTests, TrimSpaces)
{
    char buf[] = "  hello  ";
    char *r = trim(buf);
    EXPECT_STREQ(r, "hello");
}

TEST(LibTests, TrimNoSpaces)
{
    char buf[] = "hello";
    char *r = trim(buf);
    EXPECT_STREQ(r, "hello");
}

TEST(LibTests, TrimOnlySpaces)
{
    char buf[] = "     ";
    char *r = trim(buf);
    EXPECT_STREQ(r, "");
}

TEST(LibTests, TrimLeadingOnly)
{
    char buf[] = "   abc";
    char *r = trim(buf);
    EXPECT_STREQ(r, "abc");
}

TEST(LibTests, TrimTrailingOnly)
{
    char buf[] = "abc   ";
    char *r = trim(buf);
    EXPECT_STREQ(r, "abc");
}

// --- match(const char*, const char*) ---

TEST(LibTests, MatchBothNull)
{
    EXPECT_TRUE(match((const char *)nullptr, (const char *)nullptr));
}

TEST(LibTests, MatchOneNull)
{
    // NULL is converted to "", which does not match "a_b" (since "a" is required)
    EXPECT_FALSE(match((const char *)nullptr, "a_b"));
    // But NULL matches "_abc" (where nothing is required)
    EXPECT_TRUE(match((const char *)nullptr, "_abc"));
}

TEST(LibTests, MatchSimple)
{
    EXPECT_TRUE(match("read", "r_ead"));
    EXPECT_TRUE(match("r", "r_ead"));
    EXPECT_FALSE(match("x", "r_ead"));
}

TEST(LibTests, MatchStringView)
{
    std::string_view a = "read";
    std::string_view b = "r_ead";
    EXPECT_TRUE(match(a, b));
}

// --- compress ---

TEST(LibTests, CompressRemovesUnderscores)
{
    EXPECT_EQ(compress("r_ead"), "read");
    EXPECT_EQ(compress("a_b_c"), "abc");
    EXPECT_EQ(compress("hello"), "hello");
    EXPECT_EQ(compress(""), "");
}

// --- noquote ---

TEST(LibTests, NoquoteEmpty)
{
    EXPECT_EQ(noquote(""), "");
}

TEST(LibTests, NoquoteDoubleQuoted)
{
    EXPECT_EQ(noquote("\"hello\""), "hello");
}

TEST(LibTests, NoquoteSingleQuoted)
{
    EXPECT_EQ(noquote("'hello'"), "hello");
}

TEST(LibTests, NoquoteMismatch)
{
    // Opening single quote, closing doesn't match -> only strips opening
    EXPECT_EQ(noquote("'hello\""), "hello\"");
}

TEST(LibTests, NoquoteNoQuotes)
{
    EXPECT_EQ(noquote("hello"), "hello");
}

TEST(LibTests, NoquoteMatchedQuotes)
{
    // Matched double quotes with no content
    EXPECT_EQ(noquote("\"\""), "");
    // Matched single quotes with no content
    EXPECT_EQ(noquote("''"), "");
}

// --- get_idx ---

TEST(LibTests, GetIdxEmptyList)
{
    std::vector<assoc_t> list;
    EXPECT_EQ(get_idx("foo", list), nidx);
}

TEST(LibTests, GetIdxMatchFirst)
{
    std::vector<assoc_t> list;
    list.push_back(assoc_t{"t_est", "/dir"});
    EXPECT_EQ(get_idx("test", list), 0u);
}

TEST(LibTests, GetIdxMatchMiddle)
{
    std::vector<assoc_t> list;
    list.push_back(assoc_t{"", "/default"});  // default entry (empty name)
    list.push_back(assoc_t{"f_oo", "/foo"});
    list.push_back(assoc_t{"b_ar", "/bar"});
    EXPECT_EQ(get_idx("bar", list), 2u);
}

TEST(LibTests, GetIdxNoMatch)
{
    std::vector<assoc_t> list;
    list.push_back(assoc_t{"", "/default"});
    list.push_back(assoc_t{"f_oo", "/foo"});
    EXPECT_EQ(get_idx("baz", list), nidx);
}

// --- assoc_list_find ---

TEST(LibTests, AssocListFindEmpty)
{
    std::vector<assoc_t> list;
    EXPECT_EQ(assoc_list_find(list, "foo"), nullptr);
}

TEST(LibTests, AssocListFindMatch)
{
    std::vector<assoc_t> list;
    list.push_back(assoc_t{"", "/default"});
    list.push_back(assoc_t{"f_oo", "/foo"});
    const assoc_t *p = assoc_list_find(list, "foo");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->location, "/foo");
}

TEST(LibTests, AssocListFindNoMatch)
{
    std::vector<assoc_t> list;
    list.push_back(assoc_t{"", "/default"});
    list.push_back(assoc_t{"f_oo", "/foo"});
    EXPECT_EQ(assoc_list_find(list, "xyz"), nullptr);
}

// --- get_date ---

TEST(LibTests, GetDateStyle0)
{
    // Style 0 is a ctime-like format
    time_t t = 1000000000; // Sep 8 2001 or similar
    std::string result = get_date(t, 0);
    EXPECT_FALSE(result.empty());
}

TEST(LibTests, GetDateStyle1)
{
    time_t t = 1000000000;
    std::string result = get_date(t, 1);
    EXPECT_FALSE(result.empty());
    EXPECT_NE(result.find("2001"), std::string::npos);
}

TEST(LibTests, GetDateStyle6Year)
{
    time_t t = 1000000000;
    std::string result = get_date(t, 6);
    EXPECT_EQ(result, "2001");
}

TEST(LibTests, GetDateStyle14)
{
    // Compact format YYYYMMDDHHmmSS
    time_t t = 1000000000;
    std::string result = get_date(t, 14);
    EXPECT_EQ(result.size(), 14u);
    EXPECT_TRUE(result.starts_with("2001"));
}

TEST(LibTests, GetDateStyle16Hex)
{
    time_t t = 255;
    std::string result = get_date(t, 16);
    EXPECT_EQ(result, "FF");
}

TEST(LibTests, GetDateStyle18Dec)
{
    time_t t = 1000000000;
    std::string result = get_date(t, 18);
    EXPECT_EQ(result, "1000000000");
}

TEST(LibTests, GetDateNegativeStyle)
{
    time_t t = 255;
    std::string result = get_date(t, -1);
    EXPECT_EQ(result, "FF");  // negative style falls through to hex
}

TEST(LibTests, GetDateOutOfRange)
{
    time_t t = 255;
    std::string result = get_date(t, 99);
    EXPECT_EQ(result, "FF");  // out of range falls through to hex
}

// --- lower_case ---

TEST(LibTests, LowerCaseString)
{
    std::string s = "HELLO World";
    lower_case(s);
    EXPECT_EQ(s, "hello world");
}

TEST(LibTests, LowerCaseCharPtr)
{
    char buf[] = "HELLO";
    lower_case(buf);
    EXPECT_STREQ(buf, "hello");
}

TEST(LibTests, LowerCaseEmpty)
{
    std::string s;
    lower_case(s);
    EXPECT_EQ(s, "");
}
