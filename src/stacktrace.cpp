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

#ifdef OOOPSI_MSVC
#define OOOPSI_FORCE_INLINE __forceinline
#else
#define OOOPSI_FORCE_INLINE
#endif

#include <tuple> // for std::ignore

#include <cstdint>
#include <cstdio>

namespace ooopsi
{

#ifdef OOOPSI_WINDOWS
DbgHelpMutex s_dbgHelpMutex;
#endif

/**
 * Implementation of the stack collection: the handler is called for every frame.
 * Note: this function is force-inlined to avoid having it show up in the call stack.
 */
template <class Func>
OOOPSI_FORCE_INLINE size_t collectStackTrace(Func&& handler,
                                             const size_t maxStackFrames = s_MAX_STACK_FRAMES)
{
    size_t numberOfFrames = 0;

// OS-specific back trace
#ifdef OOOPSI_WINDOWS
    {
        const std::lock_guard<DbgHelpMutex> lock(s_dbgHelpMutex);
        // note: we're undecorating the name ourselves - gives much more infos!
        SymSetOptions(/*SYMOPT_UNDNAME |*/ SYMOPT_DEFERRED_LOADS);

        HANDLE thisProc = GetCurrentProcess();

        const BOOL symInitOk = SymInitialize(thisProc, NULL, TRUE);
        void* stackFrames[s_MAX_STACK_FRAMES];
        auto numFrames = std::min(s_MAX_STACK_FRAMES, maxStackFrames);
        numberOfFrames =
          RtlCaptureStackBackTrace(0, static_cast<DWORD>(numFrames), stackFrames, NULL);

        char symBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO pSymbol = reinterpret_cast<PSYMBOL_INFO>(symBuffer);

        for (size_t i = 0; i < numberOfFrames; ++i)
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

            handler(i, stackFrames[i], symName, dwDisplacement);
        }

        SymCleanup(thisProc);
    }

#elif defined(OOOPSI_LINUX)

    unw_cursor_t cursor;
    unw_context_t context;

    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    while (unw_step(&cursor) > 0)
    {
        unw_word_t offset, pc;
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        if (pc == 0)
        {
            break;
        }

        if (numberOfFrames >= maxStackFrames)
        {
            break;
        }

        char symBuffer[1024];
        const char* symName = nullptr;
        if (unw_get_proc_name(&cursor, symBuffer, sizeof(symBuffer), &offset) == 0)
        {
            symName = symBuffer;
        }

        handler(numberOfFrames, reinterpret_cast<pointer_t>(pc), symName, offset);
        numberOfFrames++;
    }

#else

#error "Unsupported platform!"

#endif // OOOPSI_WINDOWS/LINUX

    return numberOfFrames;
}


static void logFrame(const LogSettings settings, uint64_t num, pointer_t address, const char* sym,
                     uint64_t offset, const pointer_t* faultAddr)
{
    char messageBuffer[1024];
    const char* prefix = "  ";
    if (faultAddr != nullptr && *faultAddr == address)
    {
        prefix = "=>";
    }
    snprintf(messageBuffer, sizeof(messageBuffer), "%s#%-2" PRIu64 "  %p", prefix, num, address);

    if (sym != nullptr)
    {
        // append the demangled name + offset
        strncat(messageBuffer, " in ", sizeof(messageBuffer) - strlen(messageBuffer) - 1);
        size_t bufLen = strlen(messageBuffer);
        if (bufLen < sizeof(messageBuffer))
        {
            if (settings.demangleNames)
            {
                std::string demangled = demangle(sym);
                strncat(messageBuffer, demangled.c_str(), sizeof(messageBuffer) - bufLen - 1);
            }
            else
            {
                // copy the plain name (may get truncated)
#ifdef OOOPSI_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragma"
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
                strncat(messageBuffer, sym, sizeof(messageBuffer) - bufLen - 1);
#ifdef OOOPSI_GCC
#pragma GCC diagnostic pop
#endif
            }
            bufLen = strlen(messageBuffer);
        }
        if (bufLen < sizeof(messageBuffer))
        {
            snprintf(messageBuffer + bufLen, sizeof(messageBuffer) - bufLen, "+0x%" PRIx64, offset);
        }
    }
    // else: no symbol name, keep the address

    settings.logFunc(messageBuffer);
}


void printStackTrace(LogSettings settings, const pointer_t* faultAddr)
{
    if (settings.logFunc == nullptr)
    {
        settings.logFunc = getAbortLogFunc();
    }

    settings.logFunc("---------- BACKTRACE ----------");

    size_t n =
      collectStackTrace([&](uint64_t num, pointer_t address, const char* symbol, uint64_t offset) {
          logFrame(settings, num, address, symbol, offset, faultAddr);
      });
    if (n == s_MAX_STACK_FRAMES)
    {
        // the trace is (probably) truncated
        char messageBuffer[512];
        uint64_t num = n;
        snprintf(messageBuffer, sizeof(messageBuffer), "  #%-2" PRIu64 " ... (truncating)", num);
        settings.logFunc(messageBuffer);
    }

    settings.logFunc("-------------------------------");
    // END
    settings.logFunc(nullptr);
}

size_t collectStackTrace(StackFrame* buffer, size_t bufferSize) noexcept
{
    return collectStackTrace(
      [&](uint64_t num, pointer_t address, const char* symbol, uint64_t offset) {
          buffer[num].address = address;
          buffer[num].function = demangle(symbol);
          buffer[num].offset = offset;
      },
      bufferSize);
}

} // namespace ooopsi
