/**
 * @file    itanium_abi.cpp
 *
 * Replace standard library functions that log to STDERR with our variants - at least on systems
 * that follow the Itanium ABI.
 *
 * Note: These are defined in cxxabi.h and specified by the Itanium ABI.
 * Note2: The message is slightly different (upper case) to distinguish it from the default.
 */

#include "ooopsi.hpp"

#include "internal.hpp"

// For Windows, see 'handlers.cpp' - these are covered by the onTerminate(//onPureCall() functions.
#ifdef OOOPSI_LINUX
#include <cxxabi.h>
#endif // !_MSC_VER

/*
 * Replace standard library functions that log to stderr with our variants - at least on Linux.
 * These are defined in cxxabi.h and specified by the Itanium ABI.
 */

/// This function is placed in the vtable if a virtual function is "pure".
void __cxxabiv1::__cxa_pure_virtual()
{
    // never returns
    char reason[128];
    ooopsi::formatReason(reason, "PURE VIRTUAL FUNCTION CALL");
    ooopsi::abort(reason);
}

/// This function is placed in the vtable if a virtual function is "deleted".
void __cxxabiv1::__cxa_deleted_virtual()
{
    // never returns
    char reason[128];
    ooopsi::formatReason(reason, "DELETED VIRTUAL FUNCTION CALL");
    ooopsi::abort(reason);
}

#endif // OOOPSI_LINUX
