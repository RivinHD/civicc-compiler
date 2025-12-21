#include <filesystem>
#include <iostream>
#include <ostream>

#ifdef AFL_MSAN_MODE
#include <cstring>
#include <sanitizer/msan_interface.h>
#endif // AFL_MSAN_MODE

extern "C"
{
    struct node_st;
    node_st *run_scan_parse(const char *filepath);
    void cleanup_scan_parse(node_st *root);
}

int main(int argc, char *argv[])
{

// This section should be in your code that you write after all the
// necessary setup is done. It allows AFL++ to start from here in
// your main() to save time and just throw new input at the target.
#ifdef __AFL_HAVE_MANUAL_CONTROL
    __AFL_INIT();
#endif

    if (argc != 2)
    {
        std::cerr << "Missing input file argument. Usage ./civicc_scanparse <filepath>"
                  << std::endl;
        return 1;
    }

#ifdef AFL_MSAN_MODE
    __msan_unpoison(argv[1], std::strlen(argv[1]) + 1);
#endif // AFL_MSAN_MODE

    const char *filepath = argv[1];

#ifndef AFL_MSAN_MODE
    if (!std::filesystem::exists(filepath))
    {
        std::cerr << "Input file does not exists on: '" << filepath << "'" << std::endl;
        return 1;
    }
#endif // !AFL_MSAN_MODE

    node_st *root = run_scan_parse(filepath);
    cleanup_scan_parse(root);

    return 0;
}
