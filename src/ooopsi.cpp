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

[[noreturn]] void abort(const char* reason, AbortSettings settings, const pointer_t* faultAddr)
{
    if (settings.logFunc == nullptr)
    {
        settings.logFunc = getAbortLogFunc();
    }

    if (reason != nullptr)
    {
        settings.logFunc(reason);
    }

    if (settings.printStackTrace)
    {
        printStackTrace(settings, faultAddr); // NOLINT (slicing is fine here)
    }
    else
    {
        // allow logging to stop
        settings.logFunc(nullptr);
    }

    // the application will now end
    std::_Exit(OOOPSI_EXIT_CODE);
}

[[noreturn]] void abort(const char* reason, AbortSettings settings)
{
    // note: 'settings' is a POD, so std::move() has no effect here
    abort(reason, settings, nullptr);
}

} // namespace ooopsi
