#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <ostream>
#include <sys/types.h>
#include <unistd.h>

#ifdef AFL_MSAN_MODE
#include <cstring>
#include <sanitizer/msan_interface.h>
#endif // AFL_MSAN_MODE

extern "C"
{
#include "palm/ctinfo.h"
    struct node_st;
    node_st *run_scan_parse_buf(const char *filepath, char *buffer, int buffer_length);
    void cleanup_scan_parse(node_st *root);
}

/* this lets the source compile without afl-clang-fast/lto */
#ifndef __AFL_COMPILER
ssize_t fuzz_len;
const ssize_t fuzz_buf_capacity = 1024000;
unsigned char fuzz_buf[fuzz_buf_capacity];
#define __AFL_FUZZ_TESTCASE_LEN fuzz_len
#define __AFL_FUZZ_TESTCASE_BUF fuzz_buf
#define __AFL_FUZZ_INIT() void sync(void);
#define __AFL_LOOP(x) (src == NULL)
#define __AFL_INIT() sync()
#endif

__AFL_FUZZ_INIT();

/* Main entry point. */

/* To ensure checks are not optimized out it is recommended to disable
   code optimization for the fuzzer harness main() */
#pragma clang optimize off
#pragma GCC optimize("O0")

int main(int argc, char *argv[])
{
#ifndef __AFL_COMPILER
    if (argc != 2)
    {
        std::cerr << "Missing input file argument. Usage ./civicc_scanparse <filepath>"
                  << std::endl;
        return 1;
    }
#else
    (void)argc;
    (void)argv;
#endif // ! __AFL_COMPILER

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    char *src = NULL;
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;

#ifndef __AFL_COMPILER
    const char *filepath = argv[1];
    std::ifstream file(filepath, std::ios::binary);
    file.ignore(std::numeric_limits<std::streamsize>::max());
    std::streamsize size = file.gcount();
    file.clear();
    file.seekg(0, std::ios::beg);

    if (fuzz_buf_capacity < size)
    {
        std::cerr << "The file exceeds the capacity of the buffer." << std::endl;
        file.close();
        return 1;
    }

    if (!file.read((char *)fuzz_buf, size))
    {
        std::cerr << "Faild to read the file content into the buffer" << std::endl;
        file.close();
        return 1;
    }
    fuzz_len = size;
    file.close();
#else
    const char *filepath = "From AFL shared Memory";
#endif // __AFL_COMPILER

    while (__AFL_LOOP(10000))
    {
        int len = __AFL_FUZZ_TESTCASE_LEN;
        src = (char *)realloc(src, len);
        std::memcpy(src, buf, len);
        node_st *root = run_scan_parse_buf(filepath, src, len);
        cleanup_scan_parse(root);
    }

    if (src != NULL)
    {
        free(src);
    }

    CTIabortOnError();
    return 0;
}
