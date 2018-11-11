#include "ooopsi.hpp"
#include "test_helper.hpp"

#include <gtest/gtest.h>

// allow detection of wine because there are some differences...
#ifdef _WIN32
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
#else
bool isWine() noexcept
{
    return false;
}
#endif


/*
 * GoogleTest uses a different RegEx engine on Windows ... :-(
 */
#ifdef _WIN32
#define LINENUM_REGEX "\\d+"
#define BACKTRACE_REGEX_WINE ".*\n.*BACKTRACE.*"
// TODO: the regex engine is too limited to check this
//#define BACKTRACE_REGEX_REAL ".*BACKTRACE.*RtlUserThreadStart.*"
#define BACKTRACE_REGEX_REAL BACKTRACE_REGEX_WINE
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

#else
#define LINENUM_REGEX "[0-9]+"
// ensure that known symbols are found
#define BACKTRACE_REGEX ".*BACKTRACE.*__libc_start_main.*"
#define BACKTRACE_TRUNCATED_REGEX ".*BACKTRACE.*(truncating)"
#define SEGV_DETAILS "\\(address not mapped to object\\) "

static std::string makeBtRegex(const char* prefix)
{
    std::string re = prefix;
    re += BACKTRACE_REGEX;
    return re;
}

#endif

static ooopsi::HandlerSetup s_setup;

static size_t s_stackTraceNumLines = 0;
static bool s_stackTraceEndsWithNULL = false;

/**
 * Appends the given line to s_stackTrace, appending '\n'.
 * Note: not thread safe.
 *
 * @param[in] line      line text (may be nullptr)
 */
static void writeStackTrace(const char* line)
{
    s_stackTraceEndsWithNULL = (line == nullptr);
    if (line)
    {
        ++s_stackTraceNumLines;
    }
}

static void onSignal(int)
{
    ooopsi::printStackTrace(writeStackTrace, true);
}

TEST(Abort, StackTrace)
{
    // generate a stack trace

#ifdef SIGINT
    // --> for ThreadSanitizer, call it from a signal handler
    signal(SIGINT, onSignal);
    raise(SIGINT);
    signal(SIGINT, SIG_IGN);
#else
    onSignal(0);
#endif // SIGINT

    // simple comparison (this test's main intend is to have the sanitizers or valgrind check the
    // print function without crashing the current process)

    ASSERT_GE(s_stackTraceNumLines, 2);
    ASSERT_TRUE(s_stackTraceEndsWithNULL);
}

TEST(Abort, AbortDeath)
{
    // fail with some random message
    // expect a backtrace by default
    ASSERT_DEATH(ooopsi::abort("ooops"), makeBtRegex("^ooops"));
    ASSERT_DEATH(ooopsi::abort("ooops", true), makeBtRegex("^ooops"));
    // but not if disabled
    ASSERT_DEATH(ooopsi::abort("ooops", false), "^ooops\n$");
}

TEST(Abort, StdAbortDeath)
{
    ASSERT_DEATH(std::abort(), makeBtRegex("!!! TERMINATING DUE TO std::abort\\(\\)"));
}

TEST(Abort, TerminateDeath)
{
    // fail with some random message
    ASSERT_DEATH(
      failThrowStd(),
      makeBtRegex("!!! TERMINATING DUE TO std::terminate\\(\\).*std::runtime_error.*whoopsi!"));
    ASSERT_DEATH(failThrowSysErr(),
                 makeBtRegex("!!! TERMINATING DUE TO std::terminate\\(\\).*std::system_error"));
    ASSERT_DEATH(failThrowChar(),
                 makeBtRegex("!!! TERMINATING DUE TO std::terminate\\(\\).*This is my error text"));
    ASSERT_DEATH(failThrowInt(),
                 makeBtRegex("!!! TERMINATING DUE TO std::terminate\\(\\).*unknown exception"));
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
                 makeBtRegex("!!! TERMINATING DUE TO PURE VIRTUAL FUNCTION CALL"));
    ASSERT_DEATH(failDeletedVirtual(),
                 makeBtRegex("!!! TERMINATING DUE TO DELETED VIRTUAL FUNCTION CALL"));
}

#ifndef _MSC_VER // failSegmentationFault()
TEST(Abort, SegmentationFaultDeath)
{
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
        ASSERT_DEATH(failStackOverflow(),
                     "!!! TERMINATING DUE TO SEGMENTATION FAULT \\(stack overflow\\)"
#ifndef _WIN32
                     " @ 0x[0-9a-f]+" BACKTRACE_TRUNCATED_REGEX
#endif
        );
    }

#ifndef _WIN32
    // bus error
    ASSERT_DEATH(failBusError(), makeBtRegex("!!! TERMINATING DUE TO BUS ERROR"));
#endif // _WIN32
}
#endif

TEST(Abort, FloatingPointDeath)
{
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
