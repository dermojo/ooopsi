/**
 * @file    ooopsi.hpp
 *
 * This is the 'ooopsi' crash handler library.
 * See https://github.com/dermojo/ooopsi for details.
 */

#ifndef OOOPSI_HPP_
#define OOOPSI_HPP_

#include <cstddef>
#include <string>

#ifdef _MSC_VER
#define OOOPSI_DLL_EXPORT __declspec(dllexport)
#define OOOPSI_DLL_IMPORT __declspec(dllimport)
#else
#define OOOPSI_DLL_EXPORT __attribute__((visibility("default")))
#define OOOPSI_DLL_IMPORT __attribute__((visibility("default")))
#endif

#ifdef OOOPSI_BUILDING_SHARED_LIB
#define OOOPSI_EXPORT OOOPSI_DLL_EXPORT
#else
#define OOOPSI_EXPORT OOOPSI_DLL_IMPORT
#endif

namespace ooopsi
{

/// Log functions can be registered trough this function pointer. Although it cannot be
/// declared as 'noexcept', it needs to be to avoid side-effects.
/// (see setAbortLogFunc() for details)
typedef void (*LogFunc)(const char*);

/// Pointer alias. Avoid uint64_t/uintptr_t because they are a PITA when using printf.
using pointer_t = const void*;

/// Parameters for printStackTrace().
struct LogSettings
{
    /// the log function to use (nullptr: use the current handler)
    LogFunc logFunc = nullptr;
    /// demangle C++ function names? (recommended: don't in a Linux signal handler)
    bool demangleNames = true;
};

/// Parameters for abort()
struct AbortSettings : LogSettings
{
    bool printStackTrace = true;
};

/// Prints a stack trace using the given log settings.
/// The optional second argument is the address of the fault, used to highlight the according
/// line in the backtrace (if found).
///
/// Note: only throws if the log function does (and it shouldn't...) or memory allocation
/// failed during demangling.
OOOPSI_EXPORT void printStackTrace(LogSettings settings = LogSettings(),
                                   const pointer_t* faultAddr = nullptr);


/// A single stack frame.
struct StackFrame
{
    /// function address
    pointer_t address = nullptr;

    /// (de)mangled function name (if possible)
    std::string function;

    /// offset of 'address' relative to the start of the function
    size_t offset = 0;
};

/// Collects a stack trace into the given buffer.
/// Note: not safe to use in signal handlers due to the allocation of the function name.
///
/// @param[out] buffer           buffer that will be filled with stack frames
/// @param[in]  bufferSize       maximum number of frames to store in 'buffer'
/// @return number of actually stored frames in 'buffer'
OOOPSI_EXPORT size_t collectStackTrace(StackFrame* buffer, size_t bufferSize) noexcept;

/// Tries to demangle a C++ symbol (usually a function name).
/// Note: not safe to use in signal handlers due to the allocation of the function name.
///
/// @param[in] symbol        the symbol to demangle
/// @return either the demangled name or a copy of 'symbol' as fallback
OOOPSI_EXPORT std::string demangle(const char* symbol);

/// Wrapper for strings
inline std::string demangle(const std::string& symbol)
{
    return demangle(symbol.c_str());
}

/// Aborts the current process' execution, similar to std::abort, but logs a stack trace and the
/// given reason (optional).
///
/// The stack trace is logged using the log function provided in the settings (if specified), or
/// else the current log function set via setAbortFunc().
/// The default lgo function will print to STDERR.
///
/// @param reason               optional reason for program termination
/// @param settings             controls log function etc.
[[noreturn]] OOOPSI_EXPORT void abort(const char* reason, AbortSettings settings = AbortSettings());


/// Sets the log function to use when terminating due to std::terminate, std::abort or some other
/// error. The function is used the following way:
///  - It is called for every line of the stack trace (excluding trailing '\n').
///  - After the last line, it is called with a nullptr (to allow e.g. closing/flushing etc.).
///
/// Passing a nullptr restores the default log function (which prints to STDERR).
///
/// Note that this function isn't thread-safe and should be called at program startup before any
/// threads are created.
OOOPSI_EXPORT void setAbortLogFunc(LogFunc func) noexcept;

/// Returns the current log function pointer (also not thread-safe).
OOOPSI_EXPORT LogFunc getAbortLogFunc() noexcept;

/// RAII helper class to register all necessary handlers and hooks.
/// You only need this class when building a static library - the shared lib does this
/// automatically.
class OOOPSI_EXPORT HandlerSetup
{
public:
    HandlerSetup() noexcept;
    ~HandlerSetup();

    // not copyable or movable
    HandlerSetup(const HandlerSetup&) = delete;
    HandlerSetup& operator=(const HandlerSetup&) = delete;
    HandlerSetup(HandlerSetup&&) = delete;
    HandlerSetup& operator=(HandlerSetup&&) = delete;
};


} // namespace ooopsi


#endif /* OOOPSI_HPP_ */
