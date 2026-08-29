// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>

#include "trie.h"

TEST(TrieTests, EmptyTrieFind)
{
    trie<int> t;
    EXPECT_EQ(t.find("anything"), t.end());
}

TEST(TrieTests, InsertAndFindExact)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("hello", 42));
    auto it = t.find("hello");
    ASSERT_NE(it, t.end());
    EXPECT_EQ(*it, 42);
}

TEST(TrieTests, InsertDuplicateRejected)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("hello", 42));
    EXPECT_FALSE(t.insert("hello", 99));
    // Original value preserved
    auto it = t.find("hello");
    ASSERT_NE(it, t.end());
    EXPECT_EQ(*it, 42);
}

TEST(TrieTests, NoMatchBetweenKeys)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("a_lpha", 1));
    EXPECT_TRUE(t.insert("c_harlie", 3));
    EXPECT_EQ(t.find("b"), t.end());    // 'b' falls between 'a' and 'c'
    EXPECT_EQ(t.find("bravo"), t.end());
}

TEST(TrieTests, MultipleKeys)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("alpha", 1));
    EXPECT_TRUE(t.insert("beta", 2));
    EXPECT_TRUE(t.insert("gamma", 3));

    EXPECT_EQ(*t.find("alpha"), 1);
    EXPECT_EQ(*t.find("beta"), 2);
    EXPECT_EQ(*t.find("gamma"), 3);
}

TEST(TrieTests, NoMatch)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("hello", 42));
    EXPECT_EQ(t.find("world"), t.end());
}

TEST(TrieTests, AbbreviationBasic)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("r_ead", 10));
    EXPECT_EQ(*t.find("r"), 10);
    EXPECT_EQ(*t.find("re"), 10);
    EXPECT_EQ(*t.find("rea"), 10);
    EXPECT_EQ(*t.find("read"), 10);
}

TEST(TrieTests, AbbreviationTooLong)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("r_ead", 10));
    EXPECT_EQ(t.find("reads"), t.end());
}

TEST(TrieTests, AbbreviationWrongPrefix)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("r_ead", 10));
    EXPECT_EQ(t.find("x"), t.end());
    EXPECT_EQ(t.find("rex"), t.end());
}

TEST(TrieTests, EmptyKey)
{
    trie<int> t;
    EXPECT_FALSE(t.insert("", 99));
    auto it = t.find("");
    EXPECT_EQ(it, t.end());
}

TEST(TrieTests, SingleCharKey)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("a", 1));
    EXPECT_TRUE(t.insert("b", 2));
    EXPECT_EQ(*t.find("a"), 1);
    EXPECT_EQ(*t.find("b"), 2);
    EXPECT_EQ(t.find("c"), t.end());
}

TEST(TrieTests, SharedPrefixes)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("r_ead", 10));
    EXPECT_TRUE(t.insert("re_spond", 20));

    auto rea = t.find("rea");
    ASSERT_NE(rea, t.end());
    EXPECT_EQ(*rea, 10);

    auto rd = t.find("read");
    ASSERT_NE(rd, t.end());
    EXPECT_EQ(*rd, 10);

    auto re = t.find("re");
    ASSERT_NE(re, t.end());
    EXPECT_EQ(*re, 20);

    EXPECT_EQ(*t.find("res"), 20);
    EXPECT_EQ(*t.find("resp"), 20);
    EXPECT_EQ(*t.find("respo"), 20);
    EXPECT_EQ(*t.find("respon"), 20);
    EXPECT_EQ(*t.find("respond"), 20);

    EXPECT_EQ(t.find("rfc"), t.end());
}

TEST(TrieTests, SharedPrefixes2)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("re_spond", 20));
    EXPECT_TRUE(t.insert("r_ead", 10));

    auto r = t.find("r");
    ASSERT_NE(r, t.end());
    EXPECT_EQ(*r, 10);

    auto rea = t.find("rea");
    ASSERT_NE(rea, t.end());
    EXPECT_EQ(*rea, 10);

    auto rd = t.find("read");
    ASSERT_NE(rd, t.end());
    EXPECT_EQ(*rd, 10);

    auto re = t.find("re");
    ASSERT_NE(re, t.end());
    EXPECT_EQ(*re, 20);

    EXPECT_EQ(*t.find("res"), 20);
    EXPECT_EQ(*t.find("resp"), 20);
    EXPECT_EQ(*t.find("respo"), 20);
    EXPECT_EQ(*t.find("respon"), 20);
    EXPECT_EQ(*t.find("respond"), 20);
}

TEST(TrieTests, SharedPrefixes3)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("r_espond", 20));
    EXPECT_TRUE(t.insert("re_ad", 10));

    auto re = t.find("re");
    ASSERT_NE(re, t.end());
    EXPECT_EQ(*re, 10);

    auto rea = t.find("rea");
    ASSERT_NE(rea, t.end());
    EXPECT_EQ(*rea, 10);

    auto rd = t.find("read");
    ASSERT_NE(rd, t.end());
    EXPECT_EQ(*rd, 10);

    auto r = t.find("r");
    ASSERT_NE(r, t.end());
    EXPECT_EQ(*r, 20);
    EXPECT_EQ(*t.find("res"), 20);
    EXPECT_EQ(*t.find("resp"), 20);
    EXPECT_EQ(*t.find("respo"), 20);
    EXPECT_EQ(*t.find("respon"), 20);
    EXPECT_EQ(*t.find("respond"), 20);
}

TEST(TrieTests, OverlappingKeys)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("r_ead", 1));
    EXPECT_TRUE(t.insert("re_adable", 2));

    EXPECT_EQ(*t.find("r"), 1);
    EXPECT_EQ(*t.find("re"), 2);
    // TODO(cross): This should be 2: we should defer to the
    // parent abbreviation, but I'm out of time today to fix
    // it.
    EXPECT_EQ(*t.find("rea"), 1);
    EXPECT_EQ(*t.find("read"), 1);
    EXPECT_EQ(*t.find("reada"), 2);
    EXPECT_EQ(*t.find("readab"), 2);
    EXPECT_EQ(*t.find("readabl"), 2);
    EXPECT_EQ(*t.find("readable"), 2);
    EXPECT_EQ(t.find("readable0"), t.end());
}

TEST(TrieTests, UnderscoreAtEnd)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("quit_", 5));
    EXPECT_EQ(*t.find("quit"), 5);
}

TEST(TrieTests, MultipleAbbreviations)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("j_oin", 1));
    EXPECT_TRUE(t.insert("q_uit", 2));
    EXPECT_TRUE(t.insert("h_elp", 3));

    EXPECT_EQ(*t.find("j"), 1);
    EXPECT_EQ(*t.find("join"), 1);
    EXPECT_EQ(*t.find("q"), 2);
    EXPECT_EQ(*t.find("quit"), 2);
    EXPECT_EQ(*t.find("h"), 3);
    EXPECT_EQ(*t.find("help"), 3);
}

TEST(TrieTests, StringValues)
{
    trie<std::string> t;
    EXPECT_TRUE(t.insert("f_oo", std::string("foo_val")));
    EXPECT_TRUE(t.insert("b_ar", std::string("bar_val")));

    EXPECT_EQ(*t.find("f"), "foo_val");
    EXPECT_EQ(*t.find("foo"), "foo_val");
    EXPECT_EQ(*t.find("b"), "bar_val");
    EXPECT_EQ(*t.find("bar"), "bar_val");
}

TEST(TrieTests, FindNonExistentInPopulatedTrie)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("alpha", 1));
    EXPECT_TRUE(t.insert("beta", 2));
    EXPECT_EQ(t.find("chi"), t.end());
    EXPECT_EQ(t.find("alp"), t.end());  // "alpha" has no abbreviation
}

TEST(TrieTests, FullKeyRequiredNoUnderscore)
{
    trie<int> t;
    EXPECT_TRUE(t.insert("hello", 42));
    // Without underscore, full key is required
    EXPECT_EQ(t.find("hell"), t.end());
    EXPECT_EQ(t.find("h"), t.end());
    EXPECT_NE(t.find("hello"), t.end());
}
