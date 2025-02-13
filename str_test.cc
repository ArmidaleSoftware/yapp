// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "str.h"

TEST(StrTests, ToI)
{
    EXPECT_EQ(str::toi(""), 0);
    EXPECT_EQ(str::toi("a"), 0);
    EXPECT_EQ(str::toi("0"), 0);
    EXPECT_EQ(str::toi("10"), 10);
    EXPECT_EQ(str::toi("123abc"), 123);
    EXPECT_EQ(str::toi("abc123"), 0);
}

TEST(StrTests, ConcatEmpty)
{
    const std::vector<std::string_view> empty;
    EXPECT_EQ(str::concat(empty), "");
}

TEST(StrTests, ConcatSingle)
{
    const std::vector<std::string_view> one{"hi"};
    EXPECT_EQ(str::concat(one), "hi");
}

TEST(StrTests, ConcatMulti)
{
    const std::vector<std::string_view> two{"hi", "there"};
    EXPECT_EQ(str::concat(two), "hithere");
}

TEST(StrTests, JoinEmpty)
{
    const std::vector<std::string_view> empty;
    EXPECT_EQ(str::join(",", empty), "");
}

TEST(StrTests, JoinSingle)
{
    const std::vector<std::string_view> one{"single"};
    EXPECT_EQ(str::join(",", one), "single");
}

TEST(StrTests, JoinMulti)
{
    const std::vector<std::string_view> two{"one", "two"};
    EXPECT_EQ(str::join(",", two), "one,two");
}

TEST(StrTests, JoinMany)
{
    const std::vector<std::string_view> many{"1", "2", "3"};
    EXPECT_EQ(str::join(",", many), "1,2,3");
}

TEST(StrTests, TrimEmpty)
{
    EXPECT_EQ(str::ltrim(""), "");
    EXPECT_EQ(str::rtrim(""), "");
    EXPECT_EQ(str::trim(""), "");
}

TEST(StrTests, LTrim)
{
    EXPECT_EQ(str::ltrim("hi"), "hi");
    EXPECT_EQ(str::ltrim("hi "), "hi ");
    EXPECT_EQ(str::ltrim("  \t\nhi"), "hi");
    EXPECT_EQ(str::ltrim("  \t\nhi  "), "hi  ");
    EXPECT_EQ(str::ltrim("  \t\nhi there "), "hi there ");
}

TEST(StrTests, RTrim)
{
    EXPECT_EQ(str::rtrim("hi"), "hi");
    EXPECT_EQ(str::rtrim(" hi"), " hi");
    EXPECT_EQ(str::rtrim("hi \t\n \r \v \f"), "hi");
    EXPECT_EQ(str::rtrim("  hi  "), "  hi");
    EXPECT_EQ(str::rtrim(" hi there "), " hi there");
}

TEST(StrTests, Trim)
{
    EXPECT_EQ(str::trim(""), "");
    EXPECT_EQ(str::trim("   "), "");
    EXPECT_EQ(str::trim("hi"), "hi");
    EXPECT_EQ(str::trim(" hi"), "hi");
    EXPECT_EQ(str::trim("hi \t\n \r \v \f"), "hi");
    EXPECT_EQ(str::trim("  \t\nhi"), "hi");
    EXPECT_EQ(str::trim("  \t\nhi  "), "hi");
    EXPECT_EQ(str::trim("  hi  "), "hi");
    EXPECT_EQ(str::trim(" hi there "), "hi there");
}

TEST(StrTests, Eq)
{
    EXPECT_TRUE(str::eq("", ""));
    EXPECT_TRUE(str::eq("a", "a"));
    EXPECT_FALSE(str::eq("a", "b"));
    EXPECT_FALSE(str::eq("aa", "a"));
}

TEST(StrTests, EqCase)
{
    EXPECT_TRUE(str::eqcase("", ""));
    EXPECT_TRUE(str::eqcase("a", "a"));
    EXPECT_TRUE(str::eqcase("A", "a"));
    EXPECT_FALSE(str::eqcase("Ab", "aBa"));
    EXPECT_TRUE(str::eqcase("aBBcA", "AbBCa"));
}

TEST(StrTests, StartEqCase)
{
    EXPECT_TRUE(str::starteqcase("Hi there", "hi"));
    EXPECT_TRUE(str::starteqcase("Hi", "hi"));
    EXPECT_TRUE(str::starteqcase("Hi", "HI"));
    EXPECT_FALSE(str::starteqcase("Hello", "hi"));
    EXPECT_FALSE(str::starteqcase("Hi", "HI THERE"));
}

TEST(StrTests, Match)
{
    EXPECT_TRUE(str::match("", ""));
    EXPECT_TRUE(str::match("", "_a"));
    EXPECT_TRUE(str::match("a", "a"));
    EXPECT_FALSE(str::match("ab", "a"));
    EXPECT_FALSE(str::match("a", "ab_cde"));
    EXPECT_TRUE(str::match("ab", "ab_cde"));
    EXPECT_TRUE(str::match("abc", "ab_cde"));
    EXPECT_TRUE(str::match("abcd", "ab_cde"));
    EXPECT_TRUE(str::match("abcde", "ab_cde"));
    EXPECT_FALSE(str::match("abcdef", "ab_cde"));
}

TEST(StrTests, Contains)
{
    EXPECT_FALSE(str::contains({}, ""));
    EXPECT_TRUE(str::contains({""}, ""));
    EXPECT_FALSE(str::contains({"a"}, ""));
    EXPECT_FALSE(str::contains({""}, "a"));
    EXPECT_TRUE(str::contains({"a", "b", "c"}, "b"));
}

TEST(StrTests, Split)
{
    const std::vector<std::string_view> empty;
    EXPECT_EQ(str::split("", ""), empty);
    EXPECT_EQ(str::split("", ","), empty);
    EXPECT_EQ(str::split("a", ""), std::vector<std::string_view>{"a"});
    EXPECT_EQ(str::split("a", "a"), empty);
    EXPECT_EQ(str::split("a", ","), std::vector<std::string_view>{"a"});
    EXPECT_EQ(
        str::split("a,b", ","), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(
        str::split("a,,b", ","), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(
        str::split(",a,,b,", ","), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(str::split(",a, ,b,", ","),
        (std::vector<std::string_view>{"a", "", "b"}));
    EXPECT_EQ(
        str::split(",a,b", ","), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(
        str::split("a, b,", ","), (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(
        str::split("a,b,", ","), (std::vector<std::string_view>{"a", "b"}));
}

TEST(StrTests, SplitNoDiscardEmpty)
{
    const std::vector<std::string_view> empty;
    EXPECT_EQ(str::split("", "", false), empty);
    EXPECT_EQ(str::split("", ",", false), empty);
    EXPECT_EQ(str::split("a", "", false), std::vector<std::string_view>{"a"});
    EXPECT_EQ(
        str::split("a", "a", false), (std::vector<std::string_view>{"", ""}));
    EXPECT_EQ(str::split("a", ",", false), std::vector<std::string_view>{"a"});
    EXPECT_EQ(str::split("a,b", ",", false),
        (std::vector<std::string_view>{"a", "b"}));
    EXPECT_EQ(str::split("a,,,b", ",", false),
        (std::vector<std::string_view>{"a", "", "", "b"}));
    EXPECT_EQ(str::split("a,,b", ",", false),
        (std::vector<std::string_view>{"a", "", "b"}));
    EXPECT_EQ(str::split(",a,,b,", ",", false),
        (std::vector<std::string_view>{"", "a", "", "b", ""}));
    EXPECT_EQ(str::split(",a,b", ",", false),
        (std::vector<std::string_view>{"", "a", "b"}));
    EXPECT_EQ(str::split("a, b,", ",", false),
        (std::vector<std::string_view>{"a", "b", ""}));
    EXPECT_EQ(str::split("a,b,", ",", false),
        (std::vector<std::string_view>{"a", "b", ""}));
}

TEST(StrTests, UnQuote)
{
    EXPECT_EQ(str::unquote(""), "");
    EXPECT_EQ(str::unquote("'This is a test.'"), "This is a test.");
    EXPECT_EQ(
        str::unquote("'Test \\' Embedded Quote'"), "Test ' Embedded Quote");
}

TEST(StrTests, LowerCaseLiterals)
{
    EXPECT_EQ(str::lowercase(""), "");
    EXPECT_EQ(str::lowercase("asdf"), "asdf");
    EXPECT_EQ(str::lowercase("ASDF"), "asdf");
}

TEST(StrTests, LowerCaseRef)
{
    std::string s;
    str::lowercase(s);
    EXPECT_EQ(s, "");
    s = "asdf";
    EXPECT_EQ(str::lowercase(s), "asdf");
    s = "ASDF";
    EXPECT_EQ(str::lowercase(s), "asdf");
    EXPECT_EQ(s, "asdf");
}

TEST(StrTests, Strip)
{
    EXPECT_EQ(str::strip("", ""), "");
    EXPECT_EQ(str::strip("", ","), "");
    EXPECT_EQ(str::strip("hi, there,", ","), "hi there");
    EXPECT_EQ(str::strip("r_ead, or, p_ass?", " ,_?"), "readorpass");
}
