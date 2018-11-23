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
#include <thread>
#include <vector>

#ifdef OOOPSI_MINGW

// minimalistic std::thread replacement
class Thread
{
private:
    HANDLE m_handle = nullptr;
    std::function<void()> m_func;

public:
    Thread() noexcept = default;
    ~Thread()
    {
        if (joinable())
        {
            CloseHandle(m_handle);
            ooopsi::abort("un-joined thread");
        }
    }
    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;
    Thread(Thread&& rhs) { this->swap(rhs); }
    Thread& operator=(Thread&& rhs) noexcept
    {
        this->swap(rhs);
        return *this;
    }
    void swap(Thread& rhs) noexcept
    {
        using std::swap;
        swap(m_handle, rhs.m_handle);
        swap(m_func, rhs.m_func);
    }

    Thread(std::function<void()> f) : m_func(std::move(f))
    {
        m_handle = CreateThread(nullptr, 0, runThread, this, 0, NULL);
        if (m_handle == nullptr)
            throw std::runtime_error("failed to start thread");
    }

    void join()
    {
        if (m_handle == nullptr)
            throw std::runtime_error("Thread not running");

        WaitForSingleObject(m_handle, INFINITE);
        CloseHandle(m_handle);
        m_handle = nullptr;
    }

    bool joinable() const noexcept { return m_handle != nullptr; }

private:
    static DWORD WINAPI runThread(void* instance)
    {
        auto* t = reinterpret_cast<Thread*>(instance);
        t->m_func();
        return 0;
    }
};

#else

using Thread = std::thread;

#endif

class Joiner
{
public:
    Joiner(std::vector<Thread>& threads) : m_threads(threads) {}
    ~Joiner()
    {
        for (auto& t : m_threads)
        {
            if (t.joinable())
                t.join();
        }
    }

private:
    std::vector<Thread>& m_threads;
};

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
    settings.demangleNames = false;
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

// collect from multiple threads
TEST(StackTrace, CollectMultiThreaded)
{
    constexpr size_t numThreads = 5;
    std::vector<Thread> threads;
    threads.reserve(numThreads);

    Joiner tj(threads);

    for (size_t i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([] {
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
            ASSERT_EQ(last.function, "clone");
#endif
        });
    }
}
