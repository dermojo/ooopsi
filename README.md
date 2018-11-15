# ooopsi

[![Travis-CI status](https://travis-ci.org/dermojo/ooopsi.svg?branch=truncation-policy)](https://travis-ci.org/dermojo/ooopsi)

This library installs C/C++ and OS "hooks" to catch events that would crash the program:

* unhandled C++ exceptions
* segmentation faults
* stack overflows
* illegal instructions
* floating-point errors (e.g. division by zero)
* ...

Instead of crashing the process, the library logs backtrace of the current thread before
terminating the process.

"ooopsi" stands for:
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
