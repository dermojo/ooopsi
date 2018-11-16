# ooopsi

[![Travis-CI status](https://travis-ci.org/dermojo/ooopsi.svg?branch=truncation-policy)](https://travis-ci.org/dermojo/ooopsi)

This library installs C/C++ and OS hooks and handlers for events that would crash your program.
Instead of silently printing a message before terminating or sending a bug report to your OS vendor,
it allows to log a backtrace of the program's current stack and the reason why it terminates.

libooopsi currently catches:

* unhandled C++ exceptions (leading to `std::terminate`)
* segmentation faults
* stack overflows
* illegal instructions
* floating-point errors (e.g. division by zero)
* `std::abort`
* ...

![](docs/demo.gif)

The name "ooopsi" stands for:
* Out
* Of
* Options
* Print
* Stack
* Information

### Dependencies

(TODO)

* CMake
* GCC/Clang
* MSVC to be tested
* libraries
    * libunwind for Linux
    * imagehlp for Windows


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

tl;dr: Feel free to use and modify the code or to include it in your commercial application.
