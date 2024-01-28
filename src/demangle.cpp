/**
 * @file    demangle.cpp
 * @brief   function name de-mangling implementation
 */

#include "ooopsi.hpp"

#ifdef OOOPSI_MSVC
#include <windows.h>
// ensure proper include order...
#pragma warning(push)
#pragma warning(disable : 4091)
#include <dbghelp.h>
#else
#include <cxxabi.h>
#endif

#include <cstdlib>
#include <cstring>

namespace ooopsi
{

std::string demangle(const char* symbol)
{
    std::string result;

    // shall never happen, but just in case...
    if (symbol == nullptr)
    {
        return result;
    }

#ifdef _CXXABI_H

    // call GCC's demangler - will allocate a proper buffer
    int status = 0;
    char* demangledName = abi::__cxa_demangle(symbol, nullptr, nullptr, &status);
    if (demangledName != nullptr && status == 0)
    {
        result = demangledName;
    }
    else
    {
        // may not be C++, but plain C - use the original name
        result = symbol;
    }
    if (demangledName != nullptr)
    {
        free(demangledName); // NOLINT (yes, manual memory management is bad..)
    }

#elif defined(OOOPSI_MSVC)

    {
        char buffer[MAX_SYM_NAME + 1];
        memset(buffer, 0, sizeof(buffer));

        constexpr DWORD flags = UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_ACCESS_SPECIFIERS;
        const std::lock_guard<DbgHelpMutex> lock(s_dbgHelpMutex);
        if (UnDecorateSymbolName(symbol, buffer, sizeof(buffer) - 1, flags) > 0)
        {
            result = buffer;
        }
        else
        {
            result = symbol;
        }
    }

#else

    // not supported with this OS/compiler
    result = name;

#endif // _CXXABI_H

    return result;
}

} // namespace ooopsi
