#include "release_assert.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <iostream>
#include <limits>
#include <ostream>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef AFL_MSAN_MODE
#include <cstring>
#include <sanitizer/msan_interface.h>
#endif // AFL_MSAN_MODE

extern "C"
{
#include "palm/ctinfo.h"
#include "test_interface.h"
}

/* this lets the source compile without afl-clang-fast/lto */
#ifndef __AFL_COMPILER
size_t fuzz_len;
const size_t fuzz_buf_capacity = 1024000;
unsigned char fuzz_buf[fuzz_buf_capacity];
#define __AFL_FUZZ_TESTCASE_LEN fuzz_len
#define __AFL_FUZZ_TESTCASE_BUF fuzz_buf
#define __AFL_FUZZ_INIT() void sync(void);
#define __AFL_LOOP(x) (i_argc < argc)
#define __AFL_INIT() sync()
#endif

__AFL_FUZZ_INIT();

/* Main entry point. */

/* To ensure checks are not optimized out it is recommended to disable
   code optimization for the fuzzer harness main() */
#pragma clang optimize off
#pragma GCC optimize("O0")

#ifndef __AFL_COMPILER
#endif // !__AFL_COMPILER

int main(int argc, char *argv[])
{
#ifndef __AFL_COMPILER
    if (argc < 2)
    {
        std::cerr << "Missing input file argument. Usage ./civicc_<target> <filepath>+"
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

#ifndef __AFL_COMPILER
    int i_argc = 1;
#else
    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    const char *filepath = "From AFL shared Memory";
#endif // __AFL_COMPILER

    while (__AFL_LOOP(1))
    {
#ifdef __AFL_COMPILER
        size_t len = __AFL_FUZZ_TESTCASE_LEN;
        release_assert(len <= UINT32_MAX);
        release_assert(len <= 1024 * 1024 * 1024); // Ensure less than one GiB
        src = (char *)realloc(src, len);
        std::memcpy(src, buf, len);
#else
        const char *filepath = argv[i_argc++];
        printf("Compiling '%s'\n", filepath);
#endif
#if SLICE_TARGET == 1
#ifdef __AFL_COMPILER
        node_st *root = run_scan_parse_buf(filepath, src, (uint32_t)len);
#else
        node_st *root = run_scan_parse(filepath);
#endif // __AFL_COMPILER
#elif SLICE_TARGET == 2
#ifdef __AFL_COMPILER
        node_st *root = run_context_analysis_buf(filepath, src, (uint32_t)len);
#else
        node_st *root = run_context_analysis(filepath);
#endif // __AFL_COMPILER
#elif SLICE_TARGET == 3
#ifdef __AFL_COMPILER
        node_st *root = run_optimization_buf(filepath, src, (uint32_t)len);
#else
        node_st *root = run_optimization(filepath);
#endif // __AFL_COMPILER
#elif SLICE_TARGET == 4
#ifdef __AFL_COMPILER
        node_st *root = run_code_gen_preparation_buf(filepath, src, (uint32_t)len);
#else
        node_st *root = run_code_gen_preparation(filepath);
#endif // __AFL_COMPILER
#elif SLICE_TARGET == 5
#ifdef __AFL_COMPILER
        node_st *root = run_code_generation_buf(filepath, src, (uint32_t)len, NULL, NULL, 0);
#else
        node_st *root = run_code_generation(filepath, NULL, 0);
#endif // __AFL_COMPILER
#else
        static_assert(false, "No valid slice target given to preprocessor");
#endif
        cleanup_nodes(root);
    }

    if (src != NULL)
    {
        free(src);
    }

    CTIabortOnError();
    return 0;
}
