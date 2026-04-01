// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <cstring>

// Forward declare the functions we're testing (util.cc has no header)
extern char x2c(char *what);
extern void unescape_url(char *url);
extern void plustospace(char *str);
extern int rind(char *s, char c);
extern void escape_shell_cmd(char *cmd);

// --- x2c ---

TEST(UtilTests, X2cUpperA)
{
    char hex[] = "41";
    EXPECT_EQ(x2c(hex), 'A');
}

TEST(UtilTests, X2cLowerA)
{
    char hex[] = "61";
    EXPECT_EQ(x2c(hex), 'a');
}

TEST(UtilTests, X2cNewline)
{
    char hex[] = "0a";
    EXPECT_EQ(x2c(hex), '\n');
}

TEST(UtilTests, X2cUpperHex)
{
    char hex[] = "0A";
    EXPECT_EQ(x2c(hex), '\n');
}

TEST(UtilTests, X2cSpace)
{
    char hex[] = "20";
    EXPECT_EQ(x2c(hex), ' ');
}

TEST(UtilTests, X2cFF)
{
    char hex[] = "FF";
    EXPECT_EQ(x2c(hex), (char)0xFF);
}

// --- unescape_url ---

TEST(UtilTests, UnescapeUrlSpace)
{
    char url[] = "hello%20world";
    unescape_url(url);
    EXPECT_STREQ(url, "hello world");
}

TEST(UtilTests, UnescapeUrlSlash)
{
    char url[] = "a%2Fb";
    unescape_url(url);
    EXPECT_STREQ(url, "a/b");
}

TEST(UtilTests, UnescapeUrlNoEscapes)
{
    char url[] = "hello";
    unescape_url(url);
    EXPECT_STREQ(url, "hello");
}

TEST(UtilTests, UnescapeUrlMultiple)
{
    char url[] = "%41%42%43";
    unescape_url(url);
    EXPECT_STREQ(url, "ABC");
}

TEST(UtilTests, UnescapeUrlEmpty)
{
    char url[] = "";
    unescape_url(url);
    EXPECT_STREQ(url, "");
}

// --- plustospace ---

TEST(UtilTests, PlustospaceBasic)
{
    char str[] = "hello+world";
    plustospace(str);
    EXPECT_STREQ(str, "hello world");
}

TEST(UtilTests, PlustospaceNoPlus)
{
    char str[] = "hello world";
    plustospace(str);
    EXPECT_STREQ(str, "hello world");
}

TEST(UtilTests, PlustospaceMultiple)
{
    char str[] = "a+b+c+d";
    plustospace(str);
    EXPECT_STREQ(str, "a b c d");
}

TEST(UtilTests, PlustospaceEmpty)
{
    char str[] = "";
    plustospace(str);
    EXPECT_STREQ(str, "");
}

// --- rind ---

TEST(UtilTests, RindFound)
{
    char str[] = "hello world";
    EXPECT_EQ(rind(str, 'o'), 7);  // last 'o'
}

TEST(UtilTests, RindNotFound)
{
    char str[] = "hello";
    EXPECT_EQ(rind(str, 'z'), -1);
}

TEST(UtilTests, RindSingleChar)
{
    char str[] = "a";
    EXPECT_EQ(rind(str, 'a'), 0);
}

TEST(UtilTests, RindMultipleOccurrences)
{
    char str[] = "abcabc";
    EXPECT_EQ(rind(str, 'a'), 3);
}

// --- escape_shell_cmd ---

TEST(UtilTests, EscapeShellNoMeta)
{
    char cmd[256] = "hello world";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "hello world");
}

TEST(UtilTests, EscapeShellSemicolon)
{
    char cmd[256] = "ls;rm";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "ls\\;rm");
}

TEST(UtilTests, EscapeShellPipe)
{
    char cmd[256] = "cat|grep";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "cat\\|grep");
}

TEST(UtilTests, EscapeShellMultipleMeta)
{
    char cmd[256] = "a&b|c";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "a\\&b\\|c");
}

TEST(UtilTests, EscapeShellBacktick)
{
    char cmd[256] = "echo `whoami`";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "echo \\`whoami\\`");
}

TEST(UtilTests, EscapeShellDollar)
{
    char cmd[256] = "echo $HOME";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "echo \\$HOME");
}

TEST(UtilTests, EscapeShellEmpty)
{
    char cmd[256] = "";
    escape_shell_cmd(cmd);
    EXPECT_STREQ(cmd, "");
}
