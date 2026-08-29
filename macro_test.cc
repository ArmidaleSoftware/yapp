// Copyright (c) YAPP contributors
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <string>

#include "globals.h"
#include "macro.h"
#include "struct.h"
#include "yapp.h"

class MacroTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Clear all user-defined macros before each test
        undefine(DM_VAR | DM_OK | DM_PARAM | DM_RFP);
        flags = 0;
        stdin_stack_top = 0;
        orig_stdin[0].type = 0;
    }

    void TearDown() override
    {
        undefine(DM_VAR | DM_OK | DM_PARAM | DM_RFP);
    }
};

TEST_F(MacroTest, DefAndFindMacro)
{
    def_macro("testvar", DM_VAR, "hello");
    auto m = find_macro("testvar", DM_VAR);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->get().value, "hello");
}

TEST_F(MacroTest, FindMacroNotFound)
{
    auto m = find_macro("nonexistent", DM_VAR);
    EXPECT_FALSE(m.has_value());
}

TEST_F(MacroTest, FindMacroWrongMask)
{
    def_macro("testvar", DM_VAR, "hello");
    auto m = find_macro("testvar", DM_PARAM);
    EXPECT_FALSE(m.has_value());
}

TEST_F(MacroTest, UndefName)
{
    def_macro("testvar", DM_VAR, "hello");
    ASSERT_TRUE(find_macro("testvar", DM_VAR).has_value());
    undef_name("testvar");
    EXPECT_FALSE(find_macro("testvar", DM_VAR).has_value());
}

TEST_F(MacroTest, UndefineByMask)
{
    def_macro("var1", DM_VAR, "val1");
    def_macro("var2", DM_VAR, "val2");
    def_macro("param1", DM_PARAM, "pval1");

    undefine(DM_VAR);
    EXPECT_FALSE(find_macro("var1", DM_VAR).has_value());
    EXPECT_FALSE(find_macro("var2", DM_VAR).has_value());
    EXPECT_TRUE(find_macro("param1", DM_PARAM).has_value());
}

TEST_F(MacroTest, RedefMacro)
{
    def_macro("testvar", DM_VAR, "old");
    def_macro("testvar", DM_VAR, "new");
    auto m = find_macro("testvar", DM_VAR);
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->get().value, "new");
}

TEST_F(MacroTest, ReadonlyVarRejected)
{
    // "login" is in the rovars set and should be rejected
    def_macro("login", DM_VAR, "hacker");
    auto m = find_macro("login", DM_VAR);
    EXPECT_FALSE(m.has_value());
}

TEST_F(MacroTest, ZeroMaskRejected)
{
    def_macro("testvar", 0, "hello");
    auto m = find_macro("testvar", DM_VAR);
    EXPECT_FALSE(m.has_value());
}

TEST_F(MacroTest, ExpandUserDefined)
{
    def_macro("greeting", DM_VAR, "hello world");
    std::string result = expand("greeting", DM_VAR);
    EXPECT_EQ(result, "hello world");
}

TEST_F(MacroTest, ExpandNotFound)
{
    std::string result = expand("nonexistent", DM_VAR);
    EXPECT_EQ(result, "");
}

TEST_F(MacroTest, CapexpandCapitalizes)
{
    def_macro("myvar", DM_VAR, "hello");
    std::string result = capexpand("myvar", DM_VAR, true);
    EXPECT_EQ(result, "Hello");
}

TEST_F(MacroTest, CapexpandNoCap)
{
    def_macro("myvar", DM_VAR, "hello");
    std::string result = capexpand("myvar", DM_VAR, false);
    EXPECT_EQ(result, "hello");
}

TEST_F(MacroTest, CapexpandEmptyString)
{
    std::string result = capexpand("nonexistent", DM_VAR, true);
    EXPECT_EQ(result, "");
}

TEST_F(MacroTest, MultipleMacroTypes)
{
    def_macro("shared", DM_VAR, "var_value");
    def_macro("shared", DM_PARAM, "param_value");

    EXPECT_EQ(expand("shared", DM_VAR), "var_value");
    EXPECT_EQ(expand("shared", DM_PARAM), "param_value");
}
