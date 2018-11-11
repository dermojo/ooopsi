/**
 * @file    ooopsi.cpp
 * @brief
 */

#include "ooopsi.hpp"

#include <cstdio>
#ifdef _MSC_VER
#include <stdlib.h> // _exit()
#else
#include <unistd.h>
#endif

namespace ooopsi
{

#ifndef OOOPSI_EXIT_CODE
#define OOOPSI_EXIT_CODE 127
#endif // OOOPSI_EXIT_CODE

/// Default log function: prints to STDERR.
static void logToStderr(const char* message) noexcept
{
    if (message)
    {
        // fprintf(stderr, "%s\n", message);
        fputs(message, stderr);
        fputc('\n', stderr);
    }
    else
    {
        fflush(stderr);
    }
}

/// the log function to use
static LogFunc s_logFunc = logToStderr;

void setAbortLogFunc(LogFunc func) noexcept
{
    if (func)
    {
        s_logFunc = func;
    }
    else
    {
        s_logFunc = logToStderr;
    }
}

LogFunc getAbortLogFunc() noexcept
{
    return s_logFunc;
}

[[noreturn]] void abort(const char* reason, bool printTrace, bool inSignalHandler,
                        const uintptr_t* faultAddr) {
    if (reason)
    {
        s_logFunc(reason);
    }

    if (printTrace)
    {
        printStackTrace(s_logFunc, inSignalHandler, faultAddr);
    }
    else
    {
        // allow logging to stop
        s_logFunc(nullptr);
    }

    // the application will now end
    _exit(OOOPSI_EXIT_CODE);
}

  [[noreturn]] void abort(const char* reason, bool printTrace, bool inSignalHandler)
{
    abort(reason, printTrace, inSignalHandler, nullptr);
}

} // namespace ooopsi
