/**
 * @file    stacktrace.cpp
 *
 * Implements printing a stack trace on Windows and Linux.
 */

// public library header
#include "ooopsi.hpp"
// private library header
#include "internal.hpp"

#ifdef OOOPSI_WINDOWS
#include <windows.h>
// order matters..
#ifdef OOOPSI_MSVC
#pragma warning(push)
#pragma warning(disable : 4091)
#endif // OOOPSI_MSVC
#include <dbghelp.h>
#ifdef OOOPSI_MSVC
#pragma warning(pop)
#endif // OOOPSI_MSVC
#else
#include <libunwind.h>
#endif

// for compilers that support it...
#ifndef OOOPSI_MSVC
#include <cxxabi.h>
#endif

#include <mutex>
#include <tuple> // for std::ignore

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ooopsi
{

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
static DbgHelpMutex s_dbgHelpMutex;
#endif

/// buffer for __cxa_demangle() which is allocated via malloc() - unfortunately, this is required...
static char* s_demangleBuffer = nullptr;
/// allocated size of s_demangleBufferSize
static size_t s_demangleBufferSize = 0;

size_t demangle(const char* name, char* buf, const size_t bufSize)
{
    // shall never happen, but just in case...
    if (name == nullptr)
    {
        name = "<null>";
    }
    if (buf == nullptr)
    {
        return 0;
    }

#ifdef _CXXABI_H

    int status = 0;
    char* demangledName = nullptr;

    // actual demangling can only be done ion a signal-unsafe way (malloc() can't be avoided),
    // so this isn't available for signal handlers...

    if (s_demangleBuffer == nullptr)
    {
        /*
         * Initial allocation of the demangling buffer.
         * Unfortunately, __cxa_demangle() requires the use of malloc(). So lets try to allocate
         * a single buffer which shall be large enough for all subsequent mangling requests.
         *
         * Note: This buffer is intentionally leaked because this process is about to terminate
         * anyway and we don't want to mess with a failing free().
         */
        constexpr size_t initSize = 256;
        s_demangleBuffer = static_cast<char*>(malloc(initSize)); // NOLINT (malloc is required)
        // if this fails, ignore and let __cxa_demangle() handle the problem
        if (s_demangleBuffer != nullptr)
        {
            s_demangleBufferSize = initSize;
        }
    }

    // call GCC's demangler - will realloc() the buffer if needed
    demangledName = abi::__cxa_demangle(name, s_demangleBuffer, &s_demangleBufferSize, &status);

    if (demangledName != nullptr && status == 0)
    {
        // copy the demangled name
        strncpy(buf, demangledName, bufSize);
    }
    else
    {
        // may not be C++, but plain C, or demangling is disabled - use the original name
        strncpy(buf, name, bufSize);
    }

    // if the buffer was reallocated, update the pointer
    if (demangledName != nullptr)
    {
        s_demangleBuffer = demangledName;
    }
#elif defined(OOOPSI_MSVC)

    {
        const std::lock_guard<DbgHelpMutex> lock(s_dbgHelpMutex);
        if (UnDecorateSymbolName(name, buf, static_cast<DWORD>(bufSize), UNDNAME_COMPLETE) == 0)
        {
            strncpy(buf, name, bufSize);
        }
    }

#else

    // not supported with this OS/compiler
    strncpy(buf, name, bufSize);

#endif // _CXXABI_H

    return strlen(buf);
}

static void logFrame(const LogSettings settings, unsigned int num, pointer_t address,
                     const char* sym, pointer_t offset, const pointer_t* faultAddr)
{
    char messageBuffer[512];
    const char* prefix = "  ";
    if (faultAddr != nullptr && *faultAddr == address)
    {
        prefix = "=>";
    }
    snprintf(messageBuffer, sizeof(messageBuffer), "%s#%-2u  %016llx", prefix, num, address);

    if (sym != nullptr)
    {
        // append the demangled name + offset
        strncat(messageBuffer, " in ", sizeof(messageBuffer) - strlen(messageBuffer) - 1);
        size_t bufLen = strlen(messageBuffer);
        if (bufLen < sizeof(messageBuffer))
        {
            if (settings.demangleNames)
            {
                bufLen += demangle(sym, messageBuffer + bufLen, sizeof(messageBuffer) - bufLen);
            }
            else
            {
                // copy the plain name
                strncat(messageBuffer, sym, sizeof(messageBuffer) - bufLen - 1);
                bufLen = strlen(messageBuffer);
            }
        }
        if (bufLen < sizeof(messageBuffer))
        {
            snprintf(messageBuffer + bufLen, sizeof(messageBuffer) - bufLen, "+0x%llx", offset);
        }
    }
    // else: no symbol name, keep the address

    settings.logFunc(messageBuffer);
}


void printStackTrace(LogSettings settings, const pointer_t* faultAddr) noexcept
{
    if (settings.logFunc == nullptr)
    {
        settings.logFunc = getAbortLogFunc();
    }

    settings.logFunc("---------- BACKTRACE ----------");

// OS-specific back trace
#ifdef OOOPSI_WINDOWS
    {
        const std::lock_guard<DbgHelpMutex> lock(s_dbgHelpMutex);
        // note: we're undecorating the name ourselves - gives much more infos!
        SymSetOptions(/*SYMOPT_UNDNAME |*/ SYMOPT_DEFERRED_LOADS);

        HANDLE thisProc = GetCurrentProcess();

        const BOOL symInitOk = SymInitialize(thisProc, NULL, TRUE);
        void* stackFrames[s_MAX_STACK_FRAMES];
        WORD numberOfFrames = RtlCaptureStackBackTrace(0, s_MAX_STACK_FRAMES, stackFrames, NULL);

        char symBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(symBuffer);

        for (WORD i = 0; i < numberOfFrames; ++i)
        {
            const DWORD64 address = reinterpret_cast<DWORD64>(stackFrames[i]);

            DWORD64 dwDisplacement = 0;
            const char* symName = nullptr;

            if (symInitOk)
            {
                memset(symBuffer, 0, sizeof(symBuffer));
                pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                pSymbol->MaxNameLen = MAX_SYM_NAME;

                if (SymFromAddr(thisProc, address, &dwDisplacement, pSymbol))
                {
                    symName = pSymbol->Name;
                }
            }

            logFrame(settings, i, address, symName, dwDisplacement, faultAddr);
        }

        SymCleanup(thisProc);
    }

#elif defined(OOOPSI_LINUX)

    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    unsigned int i = 0;
    while (unw_step(&cursor) > 0)
    {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0)
        {
            break;
        }

        if (i >= s_MAX_STACK_FRAMES)
        {
            // we reached the maximum depth
            char messageBuffer[512];
            snprintf(messageBuffer, sizeof(messageBuffer), "  #%-2u ... (truncating)", i);
            settings.logFunc(messageBuffer);
            break;
        }

        char symBuffer[1024];
        const char* symName = nullptr;
        if (unw_get_proc_name(&cursor, symBuffer, sizeof(symBuffer), &offset) == 0)
        {
            symName = symBuffer;
        }

        logFrame(settings, i, pc, symName, offset, faultAddr);
        i++;
    }

#else

#error "Unsupported platform!"

#endif // OOOPSI_WINDOWS/LINUX

    settings.logFunc("-------------------------------");
    // END
    settings.logFunc(nullptr);
}

} // namespace ooopsi
