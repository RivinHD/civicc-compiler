#include <filesystem>
#include <iostream>
#include <ostream>
#include <string>

extern "C"
{
    struct node_st;
    node_st *run_scanner(const char *filepath);
    void cleanup_scanner(node_st *root);
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
        std::cerr << "Missing input file argument. Usage ./scanner <filepath>" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];

    if (!std::filesystem::exists(filepath))
    {
        std::cerr << "Input file does not exists on: '" << filepath << "'" << std::endl;
        return 1;
    }

    node_st *root = run_scanner(filepath.c_str());
    cleanup_scanner(root);

    return 0;
}
