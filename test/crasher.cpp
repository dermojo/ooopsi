/**
 * @file    crasher.cpp
 * @brief   Crashes in various ways...
 */
#ifdef USE_OOOPSI
#include "ooopsi.hpp"
#endif // USE_OOOPSI

#include "test_helper.hpp"

#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

class Action
{
public:
    std::string name;
    std::string description;
    std::function<void()> func;
};

int main(int argc, char** argv)
{
    // list of supported actions
    const std::vector<Action> actions = {
#ifdef USE_OOOPSI
        { "trace", "Print a stack trace", [] { ooopsi::printStackTrace(); } },
        { "abort", "Call ooopsi::abort()", [] { ooopsi::abort("ooops"); } },
#endif // USE_OOOPSI
        { "stdabort", "Call std::abort()", [] { std::abort(); } },
        { "throw-exc", "Terminate due to an uncaught std::exception", [] { failThrowStd(); } },
        { "throw-cust", "Terminate due to an uncaught custom exception",
          [] { failThrowCustomExc(); } },
        { "throw-syserr", "Terminate due to an uncaught std::system_error",
          [] { failThrowSysErr(); } },
        { "throw-char", "Terminate due to an uncaught const char*", [] { failThrowChar(); } },
        { "throw-int", "Terminate due to an uncaught int", [] { failThrowInt(); } },
        { "pure-virt", "Call a pure virtual function", [] { failPureVirtual(); } },
        { "deleted-virt", "Call a deleted virtual function", [] { failDeletedVirtual(); } },
        { "segfault", "Cause a segmentation fault", [] { failSegmentationFault(); } },
        { "stackoverflow", "Cause a stack overflow", [] { failStackOverflow(); } },
#ifndef OOOPSI_WINDOWS // not possible on Windows (AFAIK)
        { "buserror", "Cause a BUS error", [] { failBusError(); } },
#endif
        { "fpdiv", "Divide by 0", [] { failFloatingPointIntDiv(); } },
        { "illegal", "Cause an illegal instruction", [] { failIllegalInstruction(); } },
    };

    bool showHelp = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            showHelp = true;
        }
    }
    if (argc != 2 || showHelp)
    {
        std::cout << "usage: " << argv[0] << " ACTION\n";
        std::cout << "\nSupported actions:";
        std::cout << "\n------------------\n";
        for (const auto& act : actions)
        {
            std::cout << std::left << std::setw(15) << act.name << ": " << act.description << '\n';
        }
        return 1;
    }

    const char* action = argv[1];
    for (const auto& act : actions)
    {
        if (act.name == action)
        {
            act.func();
            return 0;
        }
    }

    std::cerr << "Unsupported action!\n";
    return 1;
}
