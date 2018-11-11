/**
 * @file    stacktrace.cpp
 *
 * Implements printing a stack trace on Windows and Linux.
 */

// public library header
#include "ooopsi.hpp"
// private library header
#include "internal.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// order matters..
#include <dbghelp.h>
#else
#include <libunwind.h>
#endif

#ifndef _MSC_VER
// for compilers that support it...
#include <cxxabi.h>
#endif

#include <tuple> // for std::ignore

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ooopsi
{

/// buffer for __cxa_demangle() which is allocated via malloc() - unfortunately, this is required...
static char* s_demangleBuffer = nullptr;
/// allocated size of s_demangleBufferSize
static size_t s_demangleBufferSize = 0;

size_t demangle(const char* name, char* buf, const size_t bufSize, const bool inSignalHandler)
{
    // shall never happen, but just in case...
    if (!name)
    {
        name = "<null>";
    }
    if (!buf)
    {
        return 0;
    }

#ifndef _CXXABI_H

    // not supported (yet)
    strncpy(buf, name, bufSize);

#else

    int status = 0;
    char* demangledName = nullptr;

    if (!inSignalHandler)
    {
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
            s_demangleBuffer = static_cast<char*>(malloc(initSize));
            // if this fails, ignore and let __cxa_demangle() handle the problem
            if (s_demangleBuffer)
            {
                s_demangleBufferSize = initSize;
            }
        }

        // call GCC's demangler - will realloc() the buffer if needed
        demangledName = abi::__cxa_demangle(name, s_demangleBuffer, &s_demangleBufferSize, &status);
    }
    if (demangledName && status == 0)
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
    if (demangledName)
    {
        s_demangleBuffer = demangledName;
    }
#endif // !_CXXABI_H

    return strlen(buf);
}

static void logFrame(LogFunc logFunc, const bool inSignalHandler, unsigned long num,
                     uintptr_t address, const char* sym, uintptr_t offset,
                     const uintptr_t* faultAddr)
{
    char messageBuffer[512];
    const char* prefix = "  ";
    if (faultAddr && *faultAddr == address)
        prefix = "=>";
    snprintf(messageBuffer, sizeof(messageBuffer), "%s#%-2lu  %016" PRIuPTR "", prefix, num,
             address);

    if (sym)
    {
        // append the demangled name + offset
        strncat(messageBuffer, " in ", sizeof(messageBuffer));
        size_t bufLen = strlen(messageBuffer);
        if (bufLen < sizeof(messageBuffer))
        {
            bufLen += demangle(sym, messageBuffer + bufLen, sizeof(messageBuffer) - bufLen,
                               inSignalHandler);
        }
        if (bufLen < sizeof(messageBuffer))
        {
            snprintf(messageBuffer + bufLen, sizeof(messageBuffer) - bufLen, "+0x%" PRIuPTR, offset);
        }
    }
    // else: no symbol name, keep the address

    logFunc(messageBuffer);
}


void printStackTrace(LogFunc logFunc, const bool inSignalHandler,
                     const uintptr_t* faultAddr) noexcept
{
    if (!logFunc)
    {
        logFunc = getAbortLogFunc();
    }

    logFunc("---------- BACKTRACE ----------");

// OS-specific back trace
#ifdef _WIN32
    // TODO: why undecorated? test with SYMOPT_DEBUG
    SymSetOptions(/*SYMOPT_UNDNAME |*/ SYMOPT_DEFERRED_LOADS);

    HANDLE thisProc = GetCurrentProcess();

    const BOOL symInitOk = SymInitialize(thisProc, NULL, TRUE);
    void* stackFrames[s_MAX_STACK_FRAMES];
    WORD numberOfFrames = RtlCaptureStackBackTrace(0, s_MAX_STACK_FRAMES, stackFrames, NULL);

    char symBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symBuffer;

    for (WORD i = 0; i < numberOfFrames; ++i)
    {
        const DWORD64 address = (DWORD64)stackFrames[i];

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
            else
                printf("SymFromAddr() failed,last error = %lu\n", GetLastError());
        }

        logFrame(logFunc, inSignalHandler, i, address, symName, dwDisplacement, faultAddr);
    }

    SymCleanup(thisProc);

#else
    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    size_t i = 0;
    while (unw_step(&cursor) > 0)
    {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0)
            break;

        if (i >= s_MAX_STACK_FRAMES)
        {
            // we reached the maximum depth
            char messageBuffer[512];
            snprintf(messageBuffer, sizeof(messageBuffer), "  #%-2zu ... (truncating)", i);
            logFunc(messageBuffer);
            break;
        }

        char symBuffer[1024];
        const char* symName = nullptr;
        if (unw_get_proc_name(&cursor, symBuffer, sizeof(symBuffer), &offset) == 0)
        {
            symName = symBuffer;
        }

        logFrame(logFunc, inSignalHandler, i, pc, symName, offset, faultAddr);
        i++;
    }

#endif

    logFunc("-------------------------------");
    // END
    logFunc(nullptr);
}

} // namespace ooopsi
