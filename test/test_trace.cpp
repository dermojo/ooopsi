/**
 * @file    test_trace.cpp
 *
 * Tests stack trace generation.
 */

#include "internal.hpp"
#include "ooopsi.hpp"

#ifdef OOOPSI_MSVC
// gmock triggers a warning because an ignored warning doesn't exist ... :(
#pragma warning(push)
#pragma warning(disable : 4619)
#endif
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include <csignal>

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
    ooopsi::AbortSettings settings;
    settings.logFunc = writeStackTrace;
    ooopsi::printStackTrace(settings);
}

TEST(StackTrace, Generate)
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

    ASSERT_GE(s_stackTraceNumLines, 2u);
    ASSERT_TRUE(s_stackTraceEndsWithNULL);
}

// collect into a given buffer
TEST(StackTrace, Collect)
{
    constexpr size_t maxFrames = 128;
    ooopsi::StackFrame frames[maxFrames];
    size_t numFrames = ooopsi::collectStackTrace(frames, maxFrames);
    ASSERT_LE(numFrames, maxFrames);
    ASSERT_GE(numFrames, 2);

    // look for specific frames
    using ::testing::HasSubstr;

#ifndef OOOPSI_LINUX // may get optimized out (?)
    auto& first = frames[0];
    ASSERT_THAT(first.function, HasSubstr("ooopsi::collectStackTrace("));
    ASSERT_THAT(first.function, HasSubstr("ooopsi::StackFrame"));
#endif

    auto& last = frames[numFrames - 1];
#ifdef OOOPSI_WINDOWS
    ASSERT_EQ(last.function, "RtlUserThreadStart");
#else
    ASSERT_EQ(last.function, "_start");
#endif
}
