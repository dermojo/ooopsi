/**
 * @file    handlers.cpp
 * @brief   OS-specific and generic C++ error/signal/whatever handlers
 */

// public library header
#include "ooopsi.hpp"
// private library header
#include "internal.hpp"

#include <csignal>
#include <cstring>
#include <system_error>
#include <tuple> // for std::ignore
#include <typeinfo>

#ifdef OOOPSI_WINDOWS
#include <windows.h>
#endif
#ifdef OOOPSI_LINUX
#include <sys/ucontext.h>
#endif

namespace ooopsi
{

/// Demangling is by default disabled in signal handlers: allow to overwrite this.
static bool s_forceDemangling = false;

/// Creates AbortSettings from the current context.
inline AbortSettings makeSettings(bool inSignalHandler = false) noexcept
{
    AbortSettings settings;
#ifdef OOOPSI_LINUX
    settings.demangleNames = !inSignalHandler || s_forceDemangling;
#else
    std::ignore = inSignalHandler;
#endif
    return settings;
}


#ifdef OOOPSI_WINDOWS // Windows-specific handlers

/// Windows SEH code for a C++ exception (0xE0000000 | ascii("msc"))
static constexpr DWORD s_SEH_CPP_EXCEPTION = 0xE06D7363;
/// The g++ runtime has a different code...
static constexpr DWORD s_MINGW_CPP_EXCEPTION = 0x20474343;

/**
 * Handler for Windows SEH exceptions.
 * @param[in] excInfo      information about the current exception
 */
static LONG WINAPI onWindowsException(EXCEPTION_POINTERS* excInfo)
{
    const char* exceptionType = "???";
    const char* details = nullptr;
    char detailBuf[64];
    const auto& excRec = *excInfo->ExceptionRecord;
    const pointer_t* addr = nullptr;

    switch (excRec.ExceptionCode)
    {
    default:
        // Anything not listed here is unknown or "not a problem", so skip any further action
        return EXCEPTION_CONTINUE_SEARCH;

    case s_SEH_CPP_EXCEPTION:
    case s_MINGW_CPP_EXCEPTION:
#if 0
        // TODO: analyze if we can get infos that std::current_exception should have...
        /*
         * Parameter 0 is some internal value not important to the discussion.
         * Parameter 1 is a pointer to the object being thrown (sort of).
         * Parameter 2 is a pointer to information that describes the object being thrown.
         * Parameter 3 is the HINSTANCE of the DLL that raised the exception.
         *             (Present only on 64-bit Windows.)
         */

        printf("C++ exception:\n");
        for (DWORD i = 0; i < excRec.NumberParameters; ++i)
        {
            printf("param[%lu]: %llx\n", i, excRec.ExceptionInformation[i]);
        }
#endif

        // let onTerminate() handle it
        return EXCEPTION_CONTINUE_SEARCH;

    case EXCEPTION_ACCESS_VIOLATION:
        exceptionType = "SEGMENTATION FAULT";
        if (excRec.NumberParameters >= 2)
        {
            // the first element contains a read/write flag
            // the second element contains the virtual address of the inaccessible data
            addr = reinterpret_cast<const pointer_t*>(&excRec.ExceptionInformation[1]);
        }
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        exceptionType = "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        break;
    case EXCEPTION_BREAKPOINT:
        exceptionType = "EXCEPTION_BREAKPOINT";
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        exceptionType = "EXCEPTION_DATATYPE_MISALIGNMENT";
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        exceptionType = "FLOATING POINT ERROR";
        details = "floating-point denormal operand";
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        exceptionType = "FLOATING POINT ERROR";
        details = "floating-point divide by zero";
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        exceptionType = "FLOATING POINT ERROR";
        details = " (floating-point inexact result)";
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        exceptionType = "FLOATING POINT ERROR";
        details = "floating-point invalid operation";
        break;
    case EXCEPTION_FLT_OVERFLOW:
        exceptionType = "FLOATING POINT ERROR";
        details = "floating-point overflow";
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        exceptionType = "FLOATING POINT ERROR";
        details = "floating-point stack over/underflow";
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        exceptionType = "FLOATING POINT ERROR";
        details = "floating-point underflow";
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        exceptionType = "ILLEGAL INSTRUCTION";
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        exceptionType = "PAGE ERROR";
        if (excRec.NumberParameters >= 3)
        {
            // the first element contains a read/write flag
            // the second element contains the virtual address of the inaccessible data
            // the third element contains the underlying NTSTATUS code that caused the exception
            addr = reinterpret_cast<const pointer_t*>(&excRec.ExceptionInformation[1]);
            uint64_t status = excRec.ExceptionInformation[2];
            snprintf(detailBuf, sizeof(detailBuf), "NTSTATUS=%" PRIu64, status);
            details = detailBuf;
        }
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        exceptionType = "FLOATING POINT ERROR";
        details = "integer divide by zero";
        break;
    case EXCEPTION_INT_OVERFLOW:
        exceptionType = "FLOATING POINT ERROR";
        details = "integer overflow";
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        exceptionType = "INVALID EXCEPTION HANDLER DISPOSITION";
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        exceptionType = "NONCONTINUABLE EXCEPTION";
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        exceptionType = "EXCEPTION_PRIV_INSTRUCTION";
        break;
    case EXCEPTION_SINGLE_STEP:
        exceptionType = "EXCEPTION_SINGLE_STEP";
        break;
    case EXCEPTION_STACK_OVERFLOW:
        // handled below
        break;
    }

    // only print a back trace if the stack didn't overflow... (we would only make it worse)
    if (excRec.ExceptionCode != EXCEPTION_STACK_OVERFLOW)
    {
        char reason[256];
        formatReason(reason, exceptionType, details, addr);
        abort(reason, makeSettings(), reinterpret_cast<const pointer_t*>(&excRec.ExceptionAddress));
    }
    else
    {
        AbortSettings settings;
        settings.printStackTrace = false;
        abort(REASON_PREFIX "SEGMENTATION FAULT (stack overflow)", settings);
    }
    // unreachable: return EXCEPTION_EXECUTE_HANDLER;
}


/**
 * Signal handler for Windows.
 * @param[in] sig    the signal number
 */
[[noreturn]] static void signalHandler(int sig)
{
    char reason[256];
    char buf[32];
    const char* errorType = nullptr;

    switch (sig)
    {
    case SIGABRT:
        errorType = "std::abort()";
        break;
    default:
        // should not happen, but let's handle it
        snprintf(buf, sizeof(buf), "unexpected signal %d", sig);
        errorType = buf;
        break;
    }
    formatReason(reason, errorType, nullptr, nullptr);
    abort(reason);
}

/**
 * Handles pure virtual function calls on Windows.
 */
[[noreturn]] static void onPureCall()
{
    char reason[128];
    ooopsi::formatReason(reason, "PURE/DELETED VIRTUAL FUNCTION CALL");
    ooopsi::abort(reason);
}

#else // !OOOPSI_WINDOWS

/// for systems supporting it, statically reserve it as an actual stack
static std::array<uint8_t, s_ALT_STACK_SIZE> s_ALT_STACK;

/**
 * Signal handler implementation for Linux.
 * @param[in] sig       the signal number
 * @param[in] info      additional information
 * @param[in] ctx       signal context
 */
[[noreturn]] static void signalHandler(int sig, siginfo_t* info, void* ctx)
{
    char buf[64];
    const char* what = "";
    const char* detail = nullptr;
    const pointer_t* addr = nullptr;

    // determine were we got called from
    const pointer_t* faultAddr = nullptr;
    auto* context = static_cast<const ucontext_t*>(ctx);
    if (context != nullptr)
    {
        static_assert(sizeof(greg_t) == sizeof(pointer_t), "something is odd here...");
        faultAddr = reinterpret_cast<const pointer_t*>(&context->uc_mcontext.gregs[REG_RIP]);
    }

    switch (sig)
    {
    case SIGABRT:
    {
        what = "std::abort()";
        break;
    }
    case SIGSEGV:
    {
        what = "SEGMENTATION FAULT";
        switch (info->si_code)
        {
        case SEGV_MAPERR:
            detail = "address not mapped to object";
            // may be a stack overflow...
            if (context != nullptr)
            {
                // Let's try to distinguish the usual "segmentation fault" from a
                // "stack overflow": Check if the address causing the fault is "slightly"
                // past the end of the stack.
                auto stackPtr = static_cast<uintptr_t>(context->uc_mcontext.gregs[REG_RSP]);
                auto stackAddr = reinterpret_cast<uintptr_t>(info->si_addr);
                constexpr auto rangeLimit = 2048u;
                if (stackPtr - stackAddr < rangeLimit)
                {
                    detail = "stack overflow";
                }
            }
            break;
        case SEGV_ACCERR:
            detail = "invalid permissions for mapped object";
            break;
#ifdef SEGV_BNDERR
        case SEGV_BNDERR:
            detail = "failed address bound checks";
            break;
#endif
#ifdef SEGV_PKUERR
        case SEGV_PKUERR:
            detail = "access was denied by memory protection keys";
            break;
#endif
        default:
            break;
        }
        addr = reinterpret_cast<const pointer_t*>(&info->si_addr);
        break;
    }
    case SIGBUS:
    {
        what = "BUS ERROR";
        switch (info->si_code)
        {
        case BUS_ADRALN:
            detail = "invalid address alignment";
            break;
        case BUS_ADRERR:
            detail = "nonexistent physical address";
            break;
        case BUS_OBJERR:
            detail = "object-specific hardware error";
            break;
        case BUS_MCEERR_AR:
            detail = "hardware memory error consumed on a machine check";
            break;
        case BUS_MCEERR_AO:
            detail = "hardware memory error detected in process but not consumed";
            break;
        default:
            break;
        }
        addr = reinterpret_cast<const pointer_t*>(&info->si_addr);
        break;
    }
    case SIGILL:
    {
        what = "ILLEGAL INSTRUCTION";
        switch (info->si_code)
        {
        case ILL_ILLOPC:
            detail = "illegal opcode";
            break;
        case ILL_ILLOPN:
            detail = "illegal operand";
            break;
        case ILL_ILLADR:
            detail = "illegal addressing mode";
            break;
        case ILL_ILLTRP:
            detail = "illegal trap";
            break;
        case ILL_PRVOPC:
            detail = "privileged opcode";
            break;
        case ILL_PRVREG:
            detail = "privileged register";
            break;
        case ILL_COPROC:
            detail = "coprocessor error";
            break;
        case ILL_BADSTK:
            detail = "internal stack error";
            break;
        default:
            break;
        }
        addr = reinterpret_cast<const pointer_t*>(&info->si_addr);
        break;
    }
    case SIGFPE:
    {
        what = "FLOATING POINT ERROR";
        switch (info->si_code)
        {
        case FPE_INTDIV:
            detail = "integer divide by zero";
            break;
        case FPE_INTOVF:
            detail = "integer overflow";
            break;
        case FPE_FLTDIV:
            detail = "floating-point divide by zero";
            break;
        case FPE_FLTOVF:
            detail = "floating-point overflow";
            break;
        case FPE_FLTUND:
            detail = "floating-point underflow";
            break;
        case FPE_FLTRES:
            detail = "floating-point inexact result";
            break;
        case FPE_FLTINV:
            detail = "floating-point invalid operation";
            break;
        case FPE_FLTSUB:
            detail = "subscript out of range";
            break;
        default:
            break;
        }
        addr = reinterpret_cast<const pointer_t*>(&info->si_addr);
        break;
    }
    default:
    {
        // should not happen, but let's handle it
        snprintf(buf, sizeof(buf), "unexpected signal %d", sig);
        what = buf;
        break;
    }
    }

    char reason[256];
    formatReason(reason, what, detail, addr);
    constexpr bool inSigHandler = true;
    abort(reason, makeSettings(inSigHandler), faultAddr);
}
#endif // OOOPSI_WINDOWS

/// What to do when std::terminate is called
[[noreturn]] static void onTerminate()
{
    /*
     * Note: This currently doesn't work with Visual Studio, see
     * https://developercommunity.visualstudio.com/content/problem/135332/stdcurrent-exception-returns-null-in-a-stdterminat.html
     */
    auto currentException = std::current_exception();
    if (currentException)
    {
        const char* const what = "std::terminate()";
        char detail[256];
        // if there is a current exception, re-throw it to handle its type properly
        try
        {
            std::rethrow_exception(currentException);
        }
        // extract error codes from system_error
        catch (const std::system_error& exc)
        {
            // indicate the exception's type
            const std::error_code& err = exc.code();
            snprintf(detail, sizeof(detail), "std::system_error: \"%s\" (%s:%d)",
                     err.message().c_str(), err.category().name(), err.value());
        }
        // catch-all for all standard exceptions
        catch (const std::exception& exc)
        {
            // demangle the exception class name
            std::string className = demangle(typeid(exc).name());

            // format the exception's type and error message
            snprintf(detail, sizeof(detail), "%s: \"%s\"", className.c_str(), exc.what());
        }
        // handle strings (should not be used, but who knows...)
        catch (const char* err)
        {
            if (err == nullptr)
            {
                err = "<null>";
            }

            // indicate the exception's type
            snprintf(detail, sizeof(detail), "exception (const char*): \"%s\"", err);
        }
        // anything else
        catch (...)
        {
            strncpy(detail, "unknown exception", sizeof(detail));
        }

        char reason[256];
        formatReason(reason, what, detail, nullptr);
        abort(reason);
    }
    else
    {
#ifdef OOOPSI_MINGW
        /*
         * MinGW:
         * Overwriting libstdc++'s ABI functions doesn't work here due to differences in shared
         * library loading and symbol resolution.
         * Therefore, check if we were called from __cxa_pure_*.
         */
        struct Function
        {
            const char* symbolName;
            uintptr_t symbolAddress;
            const char* abortMessage;
        };
        std::array<Function, 2> possibleCallers{
            Function{ "__cxa_pure_virtual", 0, "PURE VIRTUAL FUNCTION CALL" },
            Function{ "__cxa_deleted_virtual", 0, "DELETED VIRTUAL FUNCTION CALL" },
        };

        // only check up to 4 frames
        static constexpr DWORD lookBack = 4;
        void* stackFrames[lookBack];
        WORD numberOfFrames = RtlCaptureStackBackTrace(0, lookBack, stackFrames, NULL);
        // expect the library to be loaded - if not, it can't call us, right?
        HMODULE libstdcpp = GetModuleHandle("libstdc++-6.dll");
        if (libstdcpp)
        {
            for (auto& func : possibleCallers)
            {
                FARPROC ptr = GetProcAddress(libstdcpp, func.symbolName);
                func.symbolAddress = reinterpret_cast<uintptr_t>(ptr);
            }

            for (WORD i = 0; i < numberOfFrames; ++i)
            {
                const auto curFunc = reinterpret_cast<uintptr_t>(stackFrames[i]);
                for (const auto& func : possibleCallers)
                {
                    // check if the caller is identical or "shortly after" the function we're
                    // looking for
                    if (curFunc >= func.symbolAddress && curFunc - func.symbolAddress <= 64)
                    {
                        char reason[256];
                        pointer_t faultAddr = &stackFrames[i];
                        formatReason(reason, func.abortMessage, nullptr, &faultAddr);
                        abort(reason, makeSettings(), &faultAddr);
                    }
                }
            }
        }
#endif // OOOPSI_MINGW

        // fallback...
        char reason[256];
        formatReason(reason, "std::terminate()", nullptr, nullptr);
        abort(reason);
    }
}

static bool s_handlersRegistered = false;

// Register signal and std::terminate handlers
HandlerSetup::HandlerSetup() noexcept
{
    // allow to disable the handlers, e.g. for debugging
    const char* opt = getenv("OOOPSI_DISABLE_HANDLERS"); // flawfinder: ignore
    if (opt != nullptr && strcmp(opt, "1") == 0)
    {
        return;
    }
    // allow to enable demangling even when dumping from a signal handler
    opt = getenv("OOOPSI_FORCE_DEMANGLE"); // flawfinder: ignore
    if (opt != nullptr && strcmp(opt, "1") == 0)
    {
        s_forceDemangling = true;
    }


    if (s_handlersRegistered)
    {
        return;
    }
    s_handlersRegistered = true;

    {
        // catch std::terminate
        std::set_terminate(onTerminate);
    }

#ifdef OOOPSI_WINDOWS
    {
        // catch Windows-exceptions
        AddVectoredExceptionHandler(/* first */ 1, onWindowsException);
        // catch std::abort
        signal(SIGABRT, signalHandler);

        // install a handler for pure virtual function calls
        // (called for deleted virtual functions as well, MSVC doesn't seem to distinguish them)
        _set_purecall_handler(onPureCall);
    }
#else  // !OOOPSI_WINDOWS
    // error handler
    auto err = [](const char* what, int param) {
        char messageBuffer[256];
        snprintf(messageBuffer, sizeof(messageBuffer), "%s(%d) failed: %s", what, param,
                 strerror(errno));
        abort(messageBuffer, makeSettings());
    };

    // use an alternate stack in case we have a stack overflow!
    {
        stack_t altStack;
        altStack.ss_flags = 0;
        static_assert(s_ALT_STACK_SIZE >= MINSIGSTKSZ,
                      "Not fulfilling the system's minimum requirement!");
        altStack.ss_size = s_ALT_STACK.size();
        altStack.ss_sp = s_ALT_STACK.data();
        if (sigaltstack(&altStack, nullptr) != 0)
        {
            err("sigaltstack", s_ALT_STACK_SIZE);
        }
    }

    // catch fatal signals
    for (int sig : { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE })
    {
        // prepare the signal structure
        struct sigaction act; // NOLINT (initialization below)
        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_ONSTACK | SA_SIGINFO; // NOLINT (sorry, that's C ...)
        act.sa_sigaction = signalHandler;

        if (sigaction(sig, &act, nullptr) != 0)
        {
            err("sigaction", sig);
        }
    }
#endif // OOOPSI_WINDOWS
}

// skipping unregistering - not worth the hassle
HandlerSetup::~HandlerSetup() = default;

/// static instance for RAII-style setup
/// (not working when linking statically though...)
static HandlerSetup s_setup;

} // namespace ooopsi
