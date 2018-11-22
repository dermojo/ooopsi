/**
 * @file    test_demangle.cpp
 *
 * Demangling test cases
 */

#include "ooopsi.hpp"

#include <gtest/gtest.h>

TEST(Demangle, Nullptr)
{
    std::string result = ooopsi::demangle(nullptr);
    ASSERT_EQ(result, "");
}

TEST(Demangle, CNames)
{
    for (auto name : { "foo", "bar", "main", "this_is_not_cpp", "strlen" })
    {
        std::string result = ooopsi::demangle(name);
        ASSERT_EQ(result, name);
    }
}

TEST(Demangle, CppNames)
{
    std::string result;
#ifdef _MSC_VER
    result = ooopsi::demangle("?printStackTrace@ooopsi@@YAXULogSettings@1@PEBQEBX@Z");
    ASSERT_EQ(result,
              "void ooopsi::printStackTrace(struct ooopsi::LogSettings,void const * const *)");
    result = ooopsi::demangle("??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ");
    ASSERT_EQ(result, "std::basic_streambuf<char,struct std::char_traits<char> "
                      ">::basic_streambuf<char,struct std::char_traits<char> >(void)");
#else
    result = ooopsi::demangle("_ZNKSt16initializer_listIiE3endEv");
    ASSERT_EQ(result, "std::initializer_list<int>::end() const");
    result = ooopsi::demangle("_ZN6ooopsi15printStackTraceENS_11LogSettingsEPKPKv");
    ASSERT_EQ(result, "ooopsi::printStackTrace(ooopsi::LogSettings, void const* const*)");
#endif
}
