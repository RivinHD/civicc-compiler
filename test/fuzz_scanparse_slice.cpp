#ifdef __AFL_COMPILER
#include <assert.h>
#include <sys/mman.h>
#include <unistd.h>
#else
#include <filesystem>
#include <iostream>
#include <ostream>
#endif // __AFL_COMPILER

extern "C"
{
    struct node_st;
    node_st *run_scan_parse(const char *filepath);
    void cleanup_scan_parse(node_st *root);
}

#ifdef __AFL_COMPILER
__AFL_FUZZ_INIT();
#endif

// Also see
// https://github.com/AFLplusplus/AFLplusplus/blob/stable/instrumentation%2FREADME.persistent_mode.md
// https://nullprogram.com/blog/2025/02/05/
int main(int argc, char *argv[])
{

#ifdef __AFL_COMPILER
    (void)argc;
    (void)argv;

#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;
    int fd = memfd_create("fuzz", 0);
    assert(fd == 3);

    while (__AFL_LOOP(10000))
    {
        int len = __AFL_FUZZ_TESTCASE_LEN;

        ftruncate(fd, 0);
        pwrite(fd, buf, len, 0);
        node_st *root = run_scan_parse("/proc/self/fd/3");
        cleanup_scan_parse(root);
    }

#else
    if (argc != 2)
    {
        std::cerr << "Missing input file argument. Usage ./civicc_scanparse <filepath>"
                  << std::endl;
        return 1;
    }

    const char *filepath = argv[1];

    if (!std::filesystem::exists(filepath))
    {
        std::cerr << "Input file does not exists on: '" << filepath << "'" << std::endl;
        return 1;
    }

    node_st *root = run_scan_parse(filepath);
    cleanup_scan_parse(root);
#endif // __AFL_COMPILER

    return 0;
}
