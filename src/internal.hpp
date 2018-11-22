/**
 * @file    internal.hpp
 * @brief   library internals
 */

#ifndef INTERNAL_HPP_
#define INTERNAL_HPP_

#include "ooopsi.hpp"

#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <tuple> // for std::ignore

/*
 * OS detection
 */
#ifdef _WIN32
#define OOOPSI_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef __GNUC__
#define OOOPSI_MINGW
#endif
#endif // _WIN32

#ifdef __linux__
#define OOOPSI_LINUX
#endif // __linux__

#ifdef __clang__
#define OOOPSI_CLANG
#elif defined(__GNUC__)
#define OOOPSI_GCC
#endif

#ifdef _MSC_VER
#define OOOPSI_MSVC
#endif


#ifdef OOOPSI_WINDOWS
#include <windows.h>
#endif

// TODO: make hidden
namespace ooopsi
{

/// reserve 16KB alternate stack, allowing to put some text buffers on it
static constexpr size_t s_ALT_STACK_SIZE = 16 * 1024;

/// limits the length of the trace
static constexpr size_t s_MAX_STACK_FRAMES = 128;


/// Extension of the public abort() function with an optional address that caused the fault.
/// The address will be used to highlight the according backtrace line.
[[noreturn]] void abort(const char* reason, AbortSettings settings, const pointer_t* faultAddr);

/// define the error string prefix as a macro to allow composing compile-time messages
#define REASON_PREFIX "!!! TERMINATING DUE TO "

/// Formats a string containing the abort reason
template <size_t N>
void formatReason(char (&buffer)[N], const char* what, const char* detail = nullptr,
                  const pointer_t* addr = nullptr)
{
    snprintf(buffer, N, REASON_PREFIX "%s", what);
    if (detail)
    {
        size_t len = strlen(buffer);
        snprintf(buffer + len, N - len, " (%s)", detail);
    }
    if (addr)
    {
        size_t len = strlen(buffer);
        // avoid padding '0's here, format as non-pointer
        uintptr_t myaddr = reinterpret_cast<uintptr_t>(*addr);
        snprintf(buffer + len, N - len, " @ 0x%" PRIxPTR, myaddr);
    }
}

#ifdef OOOPSI_WINDOWS
#if defined(OOOPSI_MINGW) && !defined(_GLIBCXX_HAS_GTHREADS)
// MinGW without thread support: create a small wrapper as a workaround
class DbgHelpMutex
{
private:
    CRITICAL_SECTION m_cs;

public:
    DbgHelpMutex() { InitializeCriticalSection(&m_cs); }
    ~DbgHelpMutex() { DeleteCriticalSection(&m_cs); }

    void lock() { EnterCriticalSection(&m_cs); }
    void unlock() { LeaveCriticalSection(&m_cs); }
};
#else
using DbgHelpMutex = std::recursive_mutex;
#endif

/**
 * Mutex that guards access to all DbgHelp functions:
 *
 *  All DbgHelp functions, such as this one, are single threaded. Therefore, calls from more than
 *  one thread to this function will likely result in unexpected behavior or memory corruption.
 *  To avoid this, you must synchronize all concurrent calls from more than one thread to this
 *  function.
 */
extern DbgHelpMutex s_dbgHelpMutex;

#endif

} // namespace ooopsi


#endif /* INTERNAL_HPP_ */
