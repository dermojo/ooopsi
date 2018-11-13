/**
 * @file    ooopsi.cpp
 * @brief
 */

#include "ooopsi.hpp"

#include <cstdio>
#include <cstdlib>

namespace ooopsi
{

#ifndef OOOPSI_EXIT_CODE
#define OOOPSI_EXIT_CODE 127
#endif // OOOPSI_EXIT_CODE

/// Default log function: prints to STDERR.
static void logToStderr(const char* message) noexcept
{
    if (message != nullptr)
    {
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
    if (func != nullptr)
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
                        const pointer_t* faultAddr)
{
    if (reason != nullptr)
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
    std::_Exit(OOOPSI_EXIT_CODE);
}

[[noreturn]] void abort(const char* reason, bool printStackTrace, bool inSignalHandler)
{
    abort(reason, printStackTrace, inSignalHandler, nullptr);
}

} // namespace ooopsi
