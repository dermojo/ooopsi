/**
 * @file    test_abort.cpp
 *
 * Unit tests for the abort() function and the related hooks.
 */

#include "ooopsi.hpp"
#include "test_helper.hpp"

#include <gtest/gtest.h>

// detect compilation with AddressSanitizer: we need to exclude some bad stuff here...
#ifdef __SANITIZE_ADDRESS__
#define OOOPSI_ASAN
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OOOPSI_ASAN
#endif
#endif

// allow detection of wine because there are some differences...
#ifdef OOOPSI_WINDOWS
bool isWine() noexcept
{
    static int is_wine = -1;
    if (is_wine == -1)
    {
        // check for "wine_get_version" in ntdll
        HMODULE ntdll = GetModuleHandle("ntdll.dll");
        if (ntdll && GetProcAddress(ntdll, "wine_get_version"))
            is_wine = 1;
        else
            is_wine = 0;
    }
    return is_wine == 1;
}
#else  // !OOOPSI_WINDOWS
bool isWine() noexcept
{
    return false;
}
#endif // OOOPSI_WINDOWS


/*
 * GoogleTest uses a different RegEx engine on Windows ... :-(
 */
#ifdef OOOPSI_WINDOWS
#define LINENUM_REGEX "\\d+"
#define BACKTRACE_REGEX_WINE ".*\n.*BACKTRACE.*"
// TODO: the regex engine is too limited to check this
//#define BACKTRACE_REGEX_REAL ".*BACKTRACE.*RtlUserThreadStart.*"
#define BACKTRACE_REGEX_REAL BACKTRACE_REGEX_WINE
#define BACKTRACE_REGEX_TERM BACKTRACE_REGEX_WINE
#define BACKTRACE_TRUNCATED_REGEX BACKTRACE_REGEX_WINE
// not supported
#define SEGV_DETAILS ""

static std::string makeBtRegex(const char* prefix)
{
    std::string re = prefix;
    if (isWine())
        re += BACKTRACE_REGEX_WINE;
    else
        re += BACKTRACE_REGEX_REAL;
    return re;
}

static std::string makeBtRegexTerm(const char* prefix, const char* reason)
{
    std::string re = prefix;
    // not supported here
    std::ignore = reason;
    if (isWine())
        re += BACKTRACE_REGEX_WINE;
    else
        re += BACKTRACE_REGEX_TERM;
    return re;
}

#else // !OOOPSI_WINDOWS

#define LINENUM_REGEX "[0-9]+"
// ensure that known symbols are found
#define BACKTRACE_REGEX ".*BACKTRACE.*__libc_start_main.*"
// different call stack for std::terminate() on clang
#ifdef OOOPSI_CLANG
#define BACKTRACE_REGEX_TERMINATE ".*BACKTRACE.*__clang_call_terminate.*"
#else
#define BACKTRACE_REGEX_TERMINATE BACKTRACE_REGEX
#endif
#define BACKTRACE_TRUNCATED_REGEX ".*BACKTRACE.*(truncating)"
#define SEGV_DETAILS "\\(address not mapped to object\\) "

static std::string makeBtRegex(const char* prefix)
{
    std::string re = prefix;
    re += BACKTRACE_REGEX;
    return re;
}

static std::string makeBtRegexTerm(const char* prefix, const char* reason)
{
    std::string re = prefix;
    re += reason;
    re += BACKTRACE_REGEX_TERMINATE;
    return re;
}

#endif // OOOPSI_WINDOWS


TEST(Abort, AbortDeath)
{
    // fail with some random message
    // expect a backtrace by default
    ASSERT_DEATH(ooopsi::abort("ooops"), makeBtRegex("^ooops"));
    ASSERT_DEATH(ooopsi::abort("ooops"), makeBtRegex("^ooops"));
    // but not if disabled
    ooopsi::AbortSettings settings;
    settings.printStackTrace = false;
    ASSERT_DEATH(ooopsi::abort("ooops", settings), "^ooops\n$");
}

TEST(Abort, StdAbortDeath)
{
    ASSERT_DEATH(std::abort(), makeBtRegex("!!! TERMINATING DUE TO std::abort\\(\\)"));
}

TEST(Abort, TerminateDeath)
{
    // TODO: This fails with MSVC because std::current_exception() isn't implemented.

    // fail with some random message
    ASSERT_DEATH(failThrowStd(), makeBtRegexTerm("!!! TERMINATING DUE TO std::terminate\\(\\).*",
                                                 "std::runtime_error.*whoopsi!"));
    ASSERT_DEATH(failThrowCustomExc(),
                 makeBtRegexTerm("!!! TERMINATING DUE TO std::terminate\\(\\).*",
                                 "MyException.*this is my exception"));
    ASSERT_DEATH(failThrowSysErr(), makeBtRegexTerm("!!! TERMINATING DUE TO std::terminate\\(\\).*",
                                                    "std::system_error"));
    ASSERT_DEATH(failThrowChar(), makeBtRegexTerm("!!! TERMINATING DUE TO std::terminate\\(\\).*",
                                                  "This is my error text"));
    ASSERT_DEATH(failThrowInt(), makeBtRegexTerm("!!! TERMINATING DUE TO std::terminate\\(\\).*",
                                                 "unknown exception"));
}


TEST(Abort, CrashVirtualDeath)
{
    // note: These fail on Wine because we rely on RtlCaptureStackBackTrace...
    if (isWine())
    {
        std::cout << "[  SKIPPED ] not running virtual function crash tests on wine\n";
        return;
    }

    ASSERT_DEATH(failPureVirtual(),
                 makeBtRegex("!!! TERMINATING DUE TO PURE.* VIRTUAL FUNCTION CALL"));
    ASSERT_DEATH(failDeletedVirtual(),
                 makeBtRegex("!!! TERMINATING DUE TO .*DELETED VIRTUAL FUNCTION CALL"));
}

TEST(Abort, SegmentationFaultDeath)
{
#ifdef OOOPSI_ASAN
    GTEST_SKIP();
#endif

    // general fault
    ASSERT_DEATH(
      failSegmentationFault(),
      makeBtRegex("!!! TERMINATING DUE TO SEGMENTATION FAULT " SEGV_DETAILS "@ 0x12345678"));

    // stack overflow
    if (isWine())
    {
        // doesn't work - the exception handler isn't called
        std::cout << "[  SKIPPED ] not running stack overflow test on wine\n";
    }
    else
    {
#ifdef OOOPSI_WINDOWS
        ASSERT_DEATH(failStackOverflow(),
                     "!!! TERMINATING DUE TO SEGMENTATION FAULT \\(stack overflow\\)");
#else
        ASSERT_DEATH(failStackOverflow(),
                     "!!! TERMINATING DUE TO SEGMENTATION FAULT \\(stack overflow\\)"
                     " @ 0x[0-9a-f]+" BACKTRACE_TRUNCATED_REGEX);
#endif
    }

#ifndef OOOPSI_WINDOWS
    // bus error
    ASSERT_DEATH(failBusError(), makeBtRegex("!!! TERMINATING DUE TO BUS ERROR"));
#endif // _WIN32
}

TEST(Abort, FloatingPointDeath)
{
#ifdef OOOPSI_ASAN
    GTEST_SKIP();
#endif

    ASSERT_DEATH(
      failFloatingPointIntDiv(),
      makeBtRegex("!!! TERMINATING DUE TO FLOATING POINT ERROR \\(integer divide by zero.*"));

    // note: We can't reliably test floating-point division by zero, because the behavior is
    // implementation-defined. We currently don't get an exception/signal, but NaN. But since this
    // doesn't terminate the process, it's not a concern for this library ;-)
}

TEST(Abort, IllegalInstructionDeath)
{
    // note: don't expect a working backtrace in all situations...
    ASSERT_DEATH(failIllegalInstruction(),
                 "!!! TERMINATING DUE TO ILLEGAL INSTRUCTION.*\n.*BACKTRACE.*");
}

// TODO: multi-threaded
// TODO: Windows-specific tests
