# Requires CMake 3.2 or higher (use PPA for Travis CI)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(ooopsi VERSION 0.1 LANGUAGES CXX)

# Setup testing using GTEST
enable_testing()
find_package(GTest REQUIRED)
include_directories(SYSTEM ${GTEST_INCLUDE_DIR})

#
# Build targets
#

add_library(ooopsi
        src/ooopsi.cpp
        src/handlers.cpp
        src/itanium_abi.cpp
        src/stacktrace.cpp
    )

# Every library has unit tests, of course
add_executable(tests
        test/test_abort.cpp
    )
add_executable(example
        test/example.cpp
    )

add_test(tests tests)

# the default for ctest is very short... also the dependency to re-build tests is missing
add_custom_target(runtest COMMAND ./tests${CMAKE_EXECUTABLE_SUFFIX})
add_dependencies(runtest tests)

#
# Compiler and linker options
#

# Define headers for this library. PUBLIC headers are used for
# compiling the library, and will be added to consumers' build
# paths.
target_include_directories(ooopsi PUBLIC include)
target_include_directories(tests PRIVATE include)
target_include_directories(example PRIVATE include)

# Link test executable against gtest & gtest_main
target_link_libraries(tests ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY})
# and of course against this library
target_link_libraries(tests ooopsi)
target_link_libraries(example ooopsi)

# add libunwind for all *NIX systems
if (UNIX)
    target_link_libraries(ooopsi unwind-x86_64 unwind)
endif (UNIX)
if (WIN32)
    target_link_libraries(ooopsi imagehlp)
endif (WIN32)

set_property(TARGET ooopsi PROPERTY CXX_STANDARD 11)
set_property(TARGET ooopsi PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET tests PROPERTY CXX_STANDARD 11)
set_property(TARGET tests PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET example PROPERTY CXX_STANDARD 11)
set_property(TARGET example PROPERTY CXX_STANDARD_REQUIRED ON)

# We want a lot of warnings!
string(TOLOWER "${CMAKE_BUILD_TOOL}" MY_BUILDTOOL)
if(MY_BUILDTOOL MATCHES "(msdev|devenv|nmake|msbuild)")
    target_compile_options(tests PUBLIC /W4 /WX)
else()
    target_compile_options(tests PUBLIC -Wall -Weffc++ -Werror)
    target_link_libraries(tests pthread)
endif()
