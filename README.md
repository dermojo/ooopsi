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


## Where does the name come from?

It's an acronym for:
* Out
* Of
* Options
* Print
* Stack
* Information

Because that's all we can do then ;-)


## Dependencies and supported platforms

The library needs a C++11 compiler and supports Linux and Windows, both in 64 bit only.

To build the library, you need `libunwind-dev` on Linux and `imagehlp` on Windows as well
as CMake. The unit tests require GoogleTest, which is used as a git submodule.


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

tl;dr: Feel free to use and modify the code or to include it in your commercial application.
