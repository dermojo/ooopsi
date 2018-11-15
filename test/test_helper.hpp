/**
 * @file    test_helper.hpp
 *
 * Helper functions and classes for tests - they usually crash their caller :)
 */

#ifndef TEST_HELPER_HPP_
#define TEST_HELPER_HPP_

// include OS defines
#include "internal.hpp"

// OS-specific headers for some bad stuff...
#ifdef OOOPSI_WINDOWS
#include <windows.h>
#endif
#ifdef OOOPSI_LINUX
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <system_error>

// We need to disable optimizations for some functions, or else they won't crash... ;-)
#ifdef OOOPSI_MSVC
#define DO_NOT_OPTIMIZE __pragma(optimize("", off))
#elif defined(OOOPSI_CLANG)
#define DO_NOT_OPTIMIZE [[clang::optnone]]
#elif defined(OOOPSI_GCC)
#define DO_NOT_OPTIMIZE [[gnu::optimize("0")]]
#else
#define DO_NOT_OPTIMIZE
#endif

static void failStackOverflow()
{
    // Windows note: For wine, the error must look "recoverable", or else it will simply exit.

    // use some stack space that can't be optimized
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "now = %ld", (long)time(nullptr));
    // try to avoid Clang's warnings...
    if (buffer[0] == 'n')
        failStackOverflow();
}

static void failSegmentationFault()
{
    int* p = (int*)0x12345678;
    *p = 0;
}


#ifndef OOOPSI_WINDOWS // not possible on Windows (AFAIK)
static void runtimeError(const char* action, const char* file)
{
    std::string err = action;
    if (file)
    {
        err += " '";
        err += file;
        err += "'";
    }
    err += ": ";
    err += strerror(errno);
    throw std::runtime_error(err);
}

DO_NOT_OPTIMIZE static char failBusError()
{
    // create a paging error by truncating a memory-mapped file
    const char* const fileName = "/tmp/buserror.dat";
    FILE* f = fopen(fileName, "w+");
    if (!f)
        runtimeError("Failed to open", fileName);

    constexpr size_t size = 4096;
    const std::string content(size, 'x');
    size_t n = fwrite(content.data(), content.size(), 1, f);
    if (n != 1)
        runtimeError("Failed to write", fileName);

    // map it into memory
    if (fseek(f, 0, SEEK_SET) != 0)
        runtimeError("Failed to rewind", fileName);

    void* data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(f), 0);
    if (!data || data == MAP_FAILED)
        runtimeError("Failed to mmap", fileName);

    // now truncate it
    if (ftruncate(fileno(f), 0) != 0)
        runtimeError("Failed to truncate", fileName);
    char c = ((char*)data)[0];

    // cleanup - never reached
    munmap(data, size);
    fclose(f);

    return c;
}
#endif // OOOPSI_WINDOWS

static void failIllegalInstruction()
{
    // allocate an executable page
    constexpr size_t size = 1024;
#ifdef OOOPSI_WINDOWS
    void* page = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
    void* page =
      mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif

    // fill it with illegal instructions
    memset(page, 0xff, size);
    // execute it
    auto func = reinterpret_cast<void (*)()>(page);
    func();

// not reached
#ifdef OOOPSI_WINDOWS
    VirtualFree(page, 0, MEM_RELEASE);
#else
    munmap(page, size);
#endif
}

DO_NOT_OPTIMIZE static int failFloatingPointIntDiv()
{
    // try to avoid any clever compile-time optimizations...
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "now = %ld", (long)time(nullptr));
    int a = buffer[0] - 'n' + 1; // always 1
    int b = buffer[1] - 'o';     // always 0

    // we need to do something with the result, or Clang won't calculate it... :(
    int c = a / b;
    // note: never reached
    printf("failFloatingPointIntDiv: a=%d, b=%d, c=%d\n", a, b, c);
    return c;
}

// yes, the following code is intentionally bad :)
#ifdef OOOPSI_MSVC
#pragma warning(push)
#pragma warning(disable : 4297)
#endif
#ifdef OOOPSI_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexceptions"
#endif
#ifdef OOOPSI_GCC
#pragma GCC diagnostic push
// the error seems to be introduced in GCC 6, ignore if unknown
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wterminate"
#endif

static void failThrowStd() noexcept
{
    throw std::runtime_error("whoopsi!");
}
static void failThrowSysErr() noexcept
{
    throw std::system_error(make_error_code(std::errc::broken_pipe));
}
static void failThrowChar() noexcept
{
    throw "This is my error text";
}
static void failThrowInt() noexcept
{
    throw 42;
}
#ifdef OOOPSI_MSVC
#pragma warning(pop)
#endif
#ifdef OOOPSI_CLANG
#pragma clang diagnostic pop
#endif
#ifdef OOOPSI_GCC
#pragma GCC diagnostic pop
#endif

class FooBase;
static void doFoo(FooBase*);

/// base class with a pure virtual function
class FooBase
{
public:
    FooBase() { doFoo(this); }
    virtual ~FooBase() {}
    virtual void foo() = 0;
};

// calls the virtual function (this indirection is required, or else clang will happily
// de-virtualize the virtual call...)
DO_NOT_OPTIMIZE void doFoo(FooBase* foo)
{
    foo->foo(); // NOLINT
}

/// trigger calling a pure virtual function (from a constructor)
DO_NOT_OPTIMIZE static void failPureVirtual()
{
    /// subclass that implements the virtual function
    class FooSub : public FooBase
    {
    public:
        virtual ~FooSub() {}
        virtual void foo() override {}
    };

    // the constructor will crash
    FooSub f;
}

/// trigger calling a deleted virtual function
DO_NOT_OPTIMIZE static void failDeletedVirtual()
{
    class Foo
    {
    public:
        virtual ~Foo() = delete;
    };
    class Bar
    {
    public:
        virtual ~Bar() {}
    };

    // make sure we use Bar to avoid undefined references due to optimizations
    auto dummy = new Bar();
    delete dummy;

    // note: using placement new to avoid segmentation faults - we don't want to test these
    unsigned char buffer[128];
    auto foo = new (buffer) Foo();
    auto bar = reinterpret_cast<Bar*>(foo);
    delete bar; // NOLINT
}


#endif /* TEST_HELPER_HPP_ */
