/**
 * @file    ooopsi.hpp
 *
 * This is the 'ooopsi' crash handler library.
 * See XXX for details.
 */

#ifndef OOOPSI_HPP_
#define OOOPSI_HPP_

#include <cstdint>

namespace ooopsi
{

// TODO: DLL export / visibility

/// Log functions can be registered trough this function pointer. Although it cannot be
/// declared as 'noexcept', it needs to be to avoid side-effects.
/// (see setAbortLogFunc() for details)
typedef void (*LogFunc)(const char*);


/// Prints a stack trace using the given log function.
/// The second argument indicates whether this function is called from a signal handler.
/// In this case, the functionality is limited.
/// The optional third argument is the address of the fault, used to highlight the according
/// line in the backtrace (if found).
void printStackTrace(LogFunc logFunc, bool inSignalHandler,
                     const uintptr_t* faultAddr = nullptr) noexcept;


/// Aborts the current process' execution, similar to std::abort, but logs a stack trace and the
/// given reason (if given).
///
/// The stack trace is logged using the current log function, which can be set via setAbortFunc().
/// The default will print to STDERR.
///
/// @param reason               optional reason for the program termination
/// @param printStackTrace      whether to print a stack trace (default: yes)
/// @param inSignalHandler      must be set when called from a signal handler (default: false)
[[noreturn]] void abort(const char* reason, bool printStackTrace = true,
                        bool inSignalHandler = false);


/// Sets the log function to use when terminating due to std::terminate, std::abort or some other
/// error. The function is used the following way:
///  - It is called for every line of the stack trace (excluding trailing '\n').
///  - After the last line, it is called with a nullptr (to allow e.g. closing/flushing etc.).
///
/// Passing a nullptr restores the default log function (which prints to STDERR).
void setAbortLogFunc(LogFunc func) noexcept;

/// Returns the current log function pointer.
LogFunc getAbortLogFunc() noexcept;

/// Register signal and std::terminate handlers
class HandlerSetup
{
public:
    HandlerSetup();
    ~HandlerSetup();
};


} // namespace ooopsi


#endif /* OOOPSI_HPP_ */
