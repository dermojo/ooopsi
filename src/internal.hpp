/**
 * @file    internal.hpp
 * @brief   library internals
 */

#ifndef INTERNAL_HPP_
#define INTERNAL_HPP_

#include "ooopsi.hpp"

#include <array>
#include <cstdint>
#include <cstring>


// TODO: make hidden
namespace ooopsi
{

/*
 * OS detection
 */
#ifdef _WIN32
#define OOOPSI_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
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

static_assert(sizeof(pointer_t) >= sizeof(void*), "Need to find another pointer type!");
static_assert(sizeof(pointer_t) >= sizeof(uintptr_t), "Need to find another pointer type!");

/// reserve 16KB alternate stack, allowing to put some text buffers on it
static constexpr size_t s_ALT_STACK_SIZE = 16 * 1024;

/// limits the length of the trace
static constexpr size_t s_MAX_STACK_FRAMES = 64;

/// Tries to demangle the given symbol name to make it 'printable'. If this fails, the original
/// name is copied into the buffer.
///
/// @param[in]  name              the mangled name
/// @param[out] buf               output buffer
/// @param[in]  bufSize           size of the output buffer
/// @param[in]  inSignalHandler   whether this function is called from a signal handler
/// @return number of bytes written
size_t demangle(const char* name, char* buf, size_t bufSize, bool inSignalHandler);


/// Extension of the public abort() function with an optional address that caused the fault.
/// The address will be used to highlight the according backtrace line.
[[noreturn]] void abort(const char* reason, bool printTrace, bool inSignalHandler,
                        const pointer_t* faultAddr);

/// define the error string prefix as a macro to allow composing compile-time messages
#define REASON_PREFIX "!!! TERMINATING DUE TO "

/// Formats a string containing the abort reason
template <std::size_t N>
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
        snprintf(buffer + len, N - len, " @ 0x%llx", *addr);
    }
}

} // namespace ooopsi


#endif /* INTERNAL_HPP_ */
