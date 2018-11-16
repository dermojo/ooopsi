/**
 * @file    ooopsi.hpp
 *
 * This is the 'ooopsi' crash handler library.
 * See https://github.com/dermojo/ooopsi for details.
 */

#ifndef OOOPSI_HPP_
#define OOOPSI_HPP_

#include <cstdint>

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
using pointer_t = unsigned long long;

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

/// Prints a stack trace using the given log function.
/// The second argument indicates whether this function is called from a signal handler.
/// In this case, the functionality is limited.
/// The optional third argument is the address of the fault, used to highlight the according
/// line in the backtrace (if found).
OOOPSI_EXPORT void printStackTrace(LogSettings settings = LogSettings(),
                                   const pointer_t* faultAddr = nullptr) noexcept;


/// Aborts the current process' execution, similar to std::abort, but logs a stack trace and the
/// given reason (if given).
///
/// The stack trace is logged using the current log function, which can be set via setAbortFunc().
/// The default will print to STDERR.
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

/// RAII helper class to register signal and std::terminate handlers.
class OOOPSI_EXPORT HandlerSetup
{
public:
    HandlerSetup() noexcept;
    ~HandlerSetup();
};


} // namespace ooopsi


#endif /* OOOPSI_HPP_ */
