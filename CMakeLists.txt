# Requires CMake 3.2 or higher (use PPA for Travis CI)
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(ooopsi VERSION 0.1 LANGUAGES CXX)

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    set(gtest_force_shared_crt ON)
endif()

# Setup testing using GTEST
enable_testing()
add_subdirectory(extern/googletest)

#
# Build targets
#

add_library(ooopsi SHARED
        src/ooopsi.cpp
        src/handlers.cpp
        src/itanium_abi.cpp
        src/stacktrace.cpp
    )
target_compile_options(ooopsi PRIVATE -DOOOPSI_BUILDING_SHARED_LIB)

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
target_include_directories(tests PRIVATE include src)
target_include_directories(example PRIVATE include src)

# Link test executable against gtest & gtest_main
target_link_libraries(tests gtest_main)
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
    # Prevent deprecation errors for std::tr1 in googletest
    target_compile_options(tests PUBLIC /D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
    # Disable deprecation warnings - we need the same C functions on all platforms
    target_compile_options(ooopsi PRIVATE /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(tests PUBLIC -Wall -Weffc++ -Werror)
    target_link_libraries(tests pthread)
endif()


option(ENABLE_ASAN "Enable address sanitizer instrumentation" OFF)

if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    -fsanitize=address")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

option(ENABLE_UBSAN "Enable UB sanitizer instrumentation" OFF)

if(ENABLE_UBSAN)
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    -fsanitize=undefined")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=undefined")
endif()

option(ENABLE_COVERAGE "Enable code coverage flags" OFF)



# clang tidy support (using version 6.0 atm)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

if(NOT DEFINED CLANG_TIDY)
    set(CLANG_TIDY clang-tidy-6.0)
endif()
list(APPEND CLANG_TIDY_ARGS
    -p .
    ${CMAKE_SOURCE_DIR}/test/example.cpp
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_custom_target(
    tidy
    COMMAND ${CLANG_TIDY} ${CLANG_TIDY_ARGS}
    COMMENT "running clang tidy"
)
