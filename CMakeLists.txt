# Requires CMake 3.2 or higher
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(ooopsi VERSION 0.1 LANGUAGES CXX)

# Setup testing using GTEST
if(MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()
add_subdirectory(extern/googletest)
enable_testing()

#
# Build targets
#

add_library(ooopsi SHARED
        src/ooopsi.cpp
        src/handlers.cpp
        src/itanium_abi.cpp
        src/stacktrace.cpp
        src/demangle.cpp
    )
target_compile_options(ooopsi PRIVATE -DOOOPSI_BUILDING_SHARED_LIB)
set_target_properties(ooopsi PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(ooopsi PROPERTIES CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Every library has unit tests, of course
add_executable(tests    test/test_abort.cpp test/test_trace.cpp test/test_demangle.cpp)
# Build a crashing sample application: one copy without the lib, one with
add_executable(crasher_plain  test/crasher.cpp)
add_executable(crasher_ooopsi test/crasher.cpp)

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
target_include_directories(ooopsi         PUBLIC  include)
target_include_directories(tests          PRIVATE include src)
target_include_directories(crasher_plain  PRIVATE include src)
target_include_directories(crasher_ooopsi PRIVATE include src)

# Link test executable against gtest & gtest_main
target_link_libraries(tests gtest_main gmock)
# and of course against this library
target_link_libraries(tests ooopsi)
target_link_libraries(crasher_ooopsi ooopsi)
target_compile_options(crasher_ooopsi PRIVATE -DUSE_OOOPSI)

# add libunwind for all *NIX systems
if(UNIX)
    find_library(LIBUNWIND_LIB_PLA  unwind-${CMAKE_SYSTEM_PROCESSOR})
    find_library(LIBUNWIND_LIB_MAIN unwind)
    if(NOT LIBUNWIND_LIB_PLA OR NOT LIBUNWIND_LIB_MAIN)
        message(FATAL_ERROR "libunwind not found")
    endif()
    target_link_libraries(ooopsi ${LIBUNWIND_LIB_PLA} ${LIBUNWIND_LIB_MAIN})
endif()
if(WIN32)
    target_link_libraries(ooopsi imagehlp)
endif()

set_property(TARGET ooopsi          PROPERTY CXX_STANDARD 11)
set_property(TARGET ooopsi          PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET tests           PROPERTY CXX_STANDARD 11)
set_property(TARGET tests           PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET crasher_plain   PROPERTY CXX_STANDARD 11)
set_property(TARGET crasher_plain   PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET crasher_ooopsi  PROPERTY CXX_STANDARD 11)
set_property(TARGET crasher_ooopsi  PROPERTY CXX_STANDARD_REQUIRED ON)

# We want a lot of warnings!
# (see https://github.com/lefticus/cppbestpractices/blob/master/02-Use_the_Tools_Available.md)
if(MSVC)
    set(OOOPSI_WARNINGS /W4 /WX
                        /w14242 /w14254 /w14263 /w14265 /w14287 /we4289 /w14296 /w14311 /w14545
                        /w14546 /w14547 /w14549 /w14555 /w14619 /w14640 /w14826 /w14905 /w14906
                        /w14928)
    target_compile_options(ooopsi           PRIVATE ${OOOPSI_WARNINGS})
    target_compile_options(tests            PRIVATE ${OOOPSI_WARNINGS})
    target_compile_options(crasher_plain    PRIVATE ${OOOPSI_WARNINGS})
    target_compile_options(crasher_ooopsi   PRIVATE ${OOOPSI_WARNINGS})

    # Prevent deprecation errors for std::tr1 in googletest
    target_compile_options(tests PRIVATE /D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
    # Disable deprecation warnings - we need the same C functions on all platforms
    target_compile_options(ooopsi PRIVATE /D_CRT_SECURE_NO_WARNINGS)
else()
    set(OOOPSI_WARNINGS -Wall -Werror -Wextra -Wshadow -Wold-style-cast -Wcast-align -Wunused
                        -Wpedantic -Wconversion -Wsign-conversion -Wformat=2)
    target_compile_options(ooopsi           PRIVATE ${OOOPSI_WARNINGS})
    target_compile_options(tests            PRIVATE ${OOOPSI_WARNINGS})
    target_compile_options(crasher_plain    PRIVATE ${OOOPSI_WARNINGS})
    target_compile_options(crasher_ooopsi   PRIVATE ${OOOPSI_WARNINGS})

    target_link_libraries(tests pthread)
endif()


option(ENABLE_ASAN "Enable address sanitizer instrumentation" OFF)

if(ENABLE_ASAN)
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    -fsanitize=address")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")
endif()

option(ENABLE_TSAN "Enable thread sanitizer instrumentation" OFF)

if(ENABLE_TSAN)
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    -fsanitize=thread")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=thread")
endif()

option(ENABLE_UBSAN "Enable UB sanitizer instrumentation" OFF)

if(ENABLE_UBSAN)
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    -fsanitize=undefined")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=undefined")
endif()

option(ENABLE_COVERAGE "Enable code coverage flags" OFF)

if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS    "${CMAKE_CXX_FLAGS}    -fprofile-arcs -ftest-coverage")
    set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()


# run clang-format on all files
list(APPEND CLANG_FORMAT_ARGS
    -style=file
    -i
    ${CMAKE_SOURCE_DIR}/include/*.hpp
    ${CMAKE_SOURCE_DIR}/src/*.hpp
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/test/*.hpp
    ${CMAKE_SOURCE_DIR}/test/*.cpp
)

if(NOT DEFINED CLANG_FORMAT)
    set(CLANG_FORMAT clang-format-6.0)
endif()
add_custom_target(
    format
    COMMAND ${CLANG_FORMAT} ${CLANG_FORMAT_ARGS}
    COMMENT "running clang-format"
)


# clang tidy support (using version 6.0 atm)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)

if(NOT DEFINED CLANG_TIDY)
    set(CLANG_TIDY clang-tidy-6.0)
endif()
list(APPEND CLANG_TIDY_ARGS
    -p .
    ${CMAKE_SOURCE_DIR}/test/crasher.cpp
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

add_custom_target(
    tidy
    COMMAND ${CLANG_TIDY} ${CLANG_TIDY_ARGS}
    COMMENT "running clang tidy"
)
