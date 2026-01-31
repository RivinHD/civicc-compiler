#include "testutils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>
#ifdef __APPLE__
#include <fcntl.h>
#endif // __APPLE__

extern "C"
{
#include "palm/str.h"
#include "test_interface.h"
#include "to_string.h"
}

class GenerationTest : public testing::Test
{
  public:
    char *root_string = nullptr;
    char *symbols_string = nullptr;
    node_st *root = nullptr;
    std::string input_filepath;
    std::string err_output;
    std::string std_output;
    const uint32_t output_buffer_size = 16 * 1024 * 1024;
    // We do not want to put large memory allocation on the stack, also we want to use the benefits
    // of ASan.
    char *output_buffer = nullptr;

  protected:
    GenerationTest()
    {
        output_buffer = new char[output_buffer_size];
        output_buffer[0] = '\0';
    }

    void SetUp(node_st *node)
    {
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        root = run_code_generation_node(node, output_buffer, output_buffer_size);
        EXPECT_NE(nullptr, root) << "Could not parse ast in file: '" << input_filepath << "'";
        err_output = testing::internal::GetCapturedStderr();
        std_output = testing::internal::GetCapturedStdout();
        ASSERT_THAT(err_output,
                    testing::Not(testing::HasSubstr("error: Inconsistent node found in AST")));
        root_string = node_to_string(root);
        symbols_string = symbols_to_string(root);
        CheckAssembly();
    }

    void SetUpNoExecute(std::string filepath)
    {
        input_filepath =
            std::filesystem::absolute(std::string(PROJECT_DIRECTORY) + "/test/data/" + filepath);
        ASSERT_TRUE(std::filesystem::exists(input_filepath))
            << "File does not exist at path '" << input_filepath << "'";
    }

    void SetUp(std::string filepath)
    {
        SetUpNoExecute(filepath);
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        root = run_code_generation(input_filepath.c_str(), output_buffer, output_buffer_size);
        EXPECT_NE(nullptr, root) << "Could not parse ast in file: '" << input_filepath << "'";
        err_output = testing::internal::GetCapturedStderr();
        std_output = testing::internal::GetCapturedStdout();
        ASSERT_THAT(err_output,
                    testing::Not(testing::HasSubstr("error: Inconsistent node found in AST")));
        root_string = node_to_string(root);
        symbols_string = symbols_to_string(root);
        CheckAssembly();
    }

    void CheckAssembly()
    {
#ifdef PROGRAM_CIVAS
#ifdef __APPLE__
        const testing::TestInfo *const test_info =
            testing::UnitTest::GetInstance()->current_test_info();
        const char *testname = test_info->name();
        int proc_code = open(testname, O_RDWR);
        unlink(testname); // Immediatly remove it from the file sytem
#else                     // Linux
        int proc_code = memfd_create("test_civ_code", 0);
#endif
        ASSERT_NE(-1, proc_code);
        size_t len = STRlen(output_buffer) + 1; // Include null terminate character
        ssize_t written = pwrite(proc_code, output_buffer, len, 0);
        ASSERT_NE(-1, written);

        char *assembler_cmd =
            STRfmt("%s 2>&1 -o /dev/null /proc/self/fd/%d", PROGRAM_CIVAS, proc_code);
        FILE *fd_civas = popen(assembler_cmd, "r");
        free(assembler_cmd);
        ASSERT_NE(nullptr, fd_civas) << "Failed to popen civas";

        const size_t buf_len = 1024;
        char buf[buf_len];

        while (fgets(buf, buf_len, fd_civas) != NULL)
        {
            ASSERT_THAT(std::string(buf), testing::Not(testing::HasSubstr("error")));
        }

        // Close
        int status = pclose(fd_civas);

        ASSERT_NE(-1, status) << "Failed to retrieve the status";
        int exit_status = WEXITSTATUS(status);
        ASSERT_EQ(0, exit_status) << "Error on assembling the generated code.";

        int signal = WIFSIGNALED(status);
        ASSERT_EQ(0, signal) << "Killed by signal: '" << WTERMSIG(status) << "'";
#endif // PROGRAM_CIVAS
    }

    void TearDown() override
    {
        if (root_string != nullptr)
        {

            if (HasFailure())
            {
                std::cerr
                    << "========================================================================\n"
                    << "                        Node Representation\n"
                    << "========================================================================\n"
                    << root_string << std::endl;
            }

            free(root_string);
        }

        if (symbols_string != nullptr)
        {
            if (HasFailure())
            {
                std::cerr
                    << "========================================================================\n"
                    << "                        Symbol Tables of AST\n"
                    << "========================================================================\n"
                    << symbols_string << std::endl;
            }

            free(symbols_string);
        }

        if (root != nullptr)
        {
            cleanup_nodes(root);
            root = nullptr;
        }

        if (output_buffer != nullptr)
        {
            if (HasFailure())
            {
                std::cerr
                    << "========================================================================\n"
                    << "                            Generated Code\n"
                    << "========================================================================\n"
                    << output_buffer << std::endl;
            }
            delete[] output_buffer;
        }
    }
};

TEST_F(GenerationTest, ForLoopsGeneration)
{
    SetUp("codegen/for_loops/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 8\n"
                           "    iloadc_0\n"
                           "    istore 3\n"
                           "for0:\n"
                           "    iload_3\n"
                           ".const int 0x64  ; 100\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 1\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    istore 7\n"
                           "    iloadc_0\n"
                           "    istore 4\n"
                           "for1:\n"
                           "    iload 4\n"
                           "    iload 7\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 2\n"
                           "    istore 1\n"
                           "    iinc_1 4\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    iinc_1 3\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    iloadc_0\n"
                           "    istore 5\n"
                           "for2:\n"
                           "    iload 5\n"
                           ".const int 0xc  ; 12\n"
                           "    iloadc 3\n"
                           "    ilt\n"
                           "    branch_f endfor2\n"
                           "    iloadc_0\n"
                           "    istore 6\n"
                           "for3:\n"
                           "    iload 6\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor3\n"
                           "    iloadc 2\n"
                           "    istore 2\n"
                           ".const int 0x32  ; 50\n"
                           "    iinc 6 4\n"
                           "    jump for3\n"
                           "endfor3:\n"
                           "    iinc 5 1\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, WhileLoopsGeneration)
{
    SetUp("codegen/while_loops/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 3\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "while0:\n"
                           "    iload_0\n"
                           ".const int 0x64  ; 100\n"
                           "    iloadc 1\n"
                           "    ilt\n"
                           "    branch_f whileend0\n"
                           "    iinc_1 0\n"
                           "    jump while0\n"
                           "whileend0:\n"
                           "    iloadc_1\n"
                           "    istore 0\n"
                           "while1:\n"
                           "    iload_0\n"
                           ".const int 0x16  ; 22\n"
                           "    iloadc 2\n"
                           "    ilt\n"
                           "    branch_f whileend1\n"
                           "    iloadc_0\n"
                           "    istore 2\n"
                           "for2:\n"
                           "    iload_2\n"
                           "    iloadc 1\n"
                           "    ilt\n"
                           "    branch_f endfor2\n"
                           ".const int 0x3  ; 3\n"
                           "    iinc 1 3\n"
                           ".const int 0x32  ; 50\n"
                           "    iinc 2 4\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    iinc_1 0\n"
                           "    jump while1\n"
                           "whileend1:\n"
                           "    iload_1\n"
                           "    istore 0\n"
                           "while3:\n"
                           "    iload_0\n"
                           ".const int 0x22  ; 34\n"
                           "    iloadc 5\n"
                           "    igt\n"
                           "    branch_f whileend3\n"
                           "    idec_1 0\n"
                           "    jump while3\n"
                           "whileend3:\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, DoWhileLoopsGeneration)
{
    SetUp("codegen/do_while_loops/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 3\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "while0:\n"
                           ".const int 0x2  ; 2\n"
                           "    iinc 1 1\n"
                           "    iinc_1 0\n"
                           "    iload_0\n"
                           ".const int 0x64  ; 100\n"
                           "    iloadc 2\n"
                           "    ilt\n"
                           "    branch_t while0\n"
                           "    iloadc_1\n"
                           "    istore 0\n"
                           "while1:\n"
                           "    iloadc_0\n"
                           "    istore 2\n"
                           "for2:\n"
                           "    iload_2\n"
                           "    iloadc 2\n"
                           "    ilt\n"
                           "    branch_f endfor2\n"
                           ".const int 0x3  ; 3\n"
                           "    iinc 1 3\n"
                           ".const int 0x32  ; 50\n"
                           "    iinc 2 4\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    iinc_1 0\n"
                           "    iload_0\n"
                           ".const int 0x16  ; 22\n"
                           "    iloadc 5\n"
                           "    ilt\n"
                           "    branch_t while1\n"
                           "    iload_1\n"
                           "    istore 0\n"
                           "while3:\n"
                           "    idec_1 0\n"
                           "    iload_0\n"
                           ".const int 0x22  ; 34\n"
                           "    iloadc 6\n"
                           "    igt\n"
                           "    branch_t while3\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, BinopsGeneration)
{
    SetUp("codegen/binops/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 5\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           ".const float 0x1p-1  ; 5.000000e-01\n"
                           "    floadc 1\n"
                           "    fstore 1\n"
                           "    bloadc_t\n"
                           "    bstore 2\n"
                           "    fload_1\n"
                           ".const float 0x1.8p+2  ; 6.000000e+00\n"
                           "    floadc 2\n"
                           "    flt\n"
                           "    branch_f pfalse1\n"
                           "    fload_1\n"
                           "    floadc 2\n"
                           "    fle\n"
                           "    jump pend1\n"
                           "pfalse1:\n"
                           "    bloadc_f\n"
                           "pend1:\n"
                           "    branch_f ifend0\n"
                           "    bloadc_f\n"
                           "    bstore 2\n"
                           "ifend0:\n"
                           "    bload_2\n"
                           "    branch_f ifend2\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 3\n"
                           "    istore 0\n"
                           "ifend2:\n"
                           "    iload_0\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 4\n"
                           "    igt\n"
                           "    branch_f pfalse4\n"
                           "    bloadc_t\n"
                           "    jump pend4\n"
                           "pfalse4:\n"
                           "    iload_0\n"
                           "    iloadc_0\n"
                           "    ilt\n"
                           "pend4:\n"
                           "    branch_f ifend3\n"
                           "    floadc_1\n"
                           "    fstore 1\n"
                           "ifend3:\n"
                           "    iload_0\n"
                           "    iloadc 0\n"
                           "    imul\n"
                           "    istore 3\n"
                           "    fload_1\n"
                           ".const float 0x1p+1  ; 2.000000e+00\n"
                           "    floadc 5\n"
                           "    fdiv\n"
                           "    fstore 1\n"
                           "    iload_3\n"
                           ".const int 0x7  ; 7\n"
                           "    iloadc 6\n"
                           "    irem\n"
                           "    istore 4\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, MonopsGeneration)
{
    SetUp("codegen/monops/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 5\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    bloadc_t\n"
                           "    bstore 2\n"
                           ".const float 0x1.8p+0  ; 1.500000e+00\n"
                           "    floadc 1\n"
                           "    fstore 1\n"
                           "    bload_2\n"
                           "    bnot\n"
                           "    branch_f ifend0\n"
                           ".const float 0x1.4p+1  ; 2.500000e+00\n"
                           "    floadc 2\n"
                           "    fstore 1\n"
                           "ifend0:\n"
                           "    fload_1\n"
                           "    fneg\n"
                           "    floadc_1\n"
                           "    fmul\n"
                           "    fstore 3\n"
                           "    iload_0\n"
                           "    ineg\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 3\n"
                           "    isub\n"
                           "    istore 4\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, CastsGeneration)
{
    SetUp("codegen/casts/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 4\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           ".const float 0x1.999999999999ap+0  ; 1.600000e+00\n"
                           "    floadc 1\n"
                           "    fstore 3\n"
                           "    iload_0\n"
                           "    i2f\n"
                           "    fstore 1\n"
                           "    fload_3\n"
                           "    f2i\n"
                           "    istore 2\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, ArrayInitGeneration)
{
    SetUp("codegen/array_init/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 6\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 2\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 1\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    istore 1\n"
                           "    iloadc 1\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    bnewa\n"
                           "    astore 5\n"
                           "    bloadc_t\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_f\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_f\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_f\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_t\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_t\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_f\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_t\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_t\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_t\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_f\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    bloadc_f\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 5\n"
                           "    bstorea\n"
                           "    iload_1\n"
                           "    fnewa\n"
                           "    astore 4\n"
                           "    floadc_1\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 4\n"
                           "    fstorea\n"
                           ".const float 0x1p+1  ; 2.000000e+00\n"
                           "    floadc 3\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 4\n"
                           "    fstorea\n"
                           ".const float 0x1.8p+1  ; 3.000000e+00\n"
                           "    floadc 4\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    aload 4\n"
                           "    fstorea\n"
                           ".const float 0x1p+2  ; 4.000000e+00\n"
                           "    floadc 5\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 4\n"
                           "    fstorea\n"
                           ".const float 0x1.4p+2  ; 5.000000e+00\n"
                           "    floadc 6\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 4\n"
                           "    fstorea\n"
                           ".const float 0x1.8p+2  ; 6.000000e+00\n"
                           "    floadc 7\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    aload 4\n"
                           "    fstorea\n"
                           "    iload_2\n"
                           "    inewa\n"
                           "    astore 3\n"
                           "    iloadc_1\n"
                           "    iloadc_0\n"
                           "    aload_3\n"
                           "    istorea\n"
                           "    iloadc 1\n"
                           "    iloadc_1\n"
                           "    aload_3\n"
                           "    istorea\n"
                           "    iloadc 2\n"
                           "    iloadc 1\n"
                           "    aload_3\n"
                           "    istorea\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 8\n"
                           "    iloadc 2\n"
                           "    aload_3\n"
                           "    istorea\n"
                           "    iloadc 0\n"
                           "    iloadc 8\n"
                           "    aload_3\n"
                           "    istorea\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, ExternGeneration)
{
    SetUp("codegen/extern/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importvar \"a\" int\n"
                           ".importvar \"b\" float\n"
                           ".importvar \"c\" bool\n"
                           ".global int\n"
                           ".importvar \"__dim0_arr1\" int\n"
                           ".importvar \"arr1\" int[]\n"
                           ".importvar \"__dim0_arr2\" int\n"
                           ".importvar \"__dim1_arr2\" int\n"
                           ".importvar \"arr2\" float[]\n"
                           ".importvar \"__dim0_arr3\" int\n"
                           ".importvar \"__dim1_arr3\" int\n"
                           ".importvar \"__dim2_arr3\" int\n"
                           ".importvar \"arr3\" bool[]\n"
                           ".importfun \"fun1\" void\n"
                           ".importfun \"fun2\" int\n"
                           ".importfun \"fun3\" float\n"
                           ".importfun \"fun4\" bool\n"
                           ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 3\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istoree 0\n"
                           "    iloadc_0\n"
                           "    aloade 4\n"
                           "    iloada\n"
                           "    iloade 0\n"
                           "    iadd\n"
                           "    istore 0\n"
                           "    isrg\n"
                           "    jsre 1\n"
                           "    iload_0\n"
                           "    iadd\n"
                           "    istore 1\n"
                           "    iload_1\n"
                           "    iloadg 0\n"
                           "    iadd\n"
                           "    istore 2\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 1\n"
                           "    istoreg 0\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, ManualExample)
{
    SetUp("codegen/manual_example/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importfun \"printInt\" void int\n"
                           ".importfun \"scanInt\" int\n"
                           ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 3\n"
                           "    isrg\n"
                           "    jsre 1\n"
                           "    istore 1\n"
                           "    iload_1\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    inewa\n"
                           "    astore 2\n"
                           "    isrg\n"
                           "    iload_1\n"
                           "    aload_2\n"
                           "    jsr 2 readValues\n"
                           "    isrg\n"
                           "    iload_1\n"
                           "    aload_2\n"
                           "    jsr 2 printValues\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           "readValues:\n"
                           "    esr 2\n"
                           "    iload_0\n"
                           "    istore 3\n"
                           "    iloadc_0\n"
                           "    istore 2\n"
                           "for0:\n"
                           "    iload_2\n"
                           "    iload_3\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           "    isrg\n"
                           "    jsre 1\n"
                           "    iload_2\n"
                           "    aload_1\n"
                           "    istorea\n"
                           "    iinc_1 2\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    return\n"
                           "printValues:\n"
                           "    esr 2\n"
                           "    iload_0\n"
                           "    istore 3\n"
                           "    iloadc_0\n"
                           "    istore 2\n"
                           "for1:\n"
                           "    iload_2\n"
                           "    iload_3\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           "    isrg\n"
                           "    iload_2\n"
                           "    aload_1\n"
                           "    iloada\n"
                           "    jsre 0\n"
                           "    iinc_1 2\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Typecheck_Valid)
{
    SetUp("typecheck/valid/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "ops:\n"
                           "    esr 2\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           ".const int 0x7  ; 7\n"
                           "    iloadc 2\n"
                           "    igt\n"
                           "    bstore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 0\n"
                           "    ineg\n"
                           "    isub\n"
                           "    istore 1\n"
                           ".const float 0x1.4p+2  ; 5.000000e+00\n"
                           "    floadc 3\n"
                           ".const float 0x1.8p+1  ; 3.000000e+00\n"
                           "    floadc 4\n"
                           "    fadd\n"
                           ".const float 0x1.cp+2  ; 7.000000e+00\n"
                           "    floadc 5\n"
                           "    fne\n"
                           "    bstore 0\n"
                           "    floadc 3\n"
                           "    floadc 4\n"
                           "    fsub\n"
                           "    floadc 5\n"
                           "    feq\n"
                           "    bstore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    ine\n"
                           "    bstore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 1\n"
                           "    isub\n"
                           "    iloadc 2\n"
                           "    ieq\n"
                           "    bstore 0\n"
                           "    bloadc_t\n"
                           "    bload_0\n"
                           "    bne\n"
                           "    bstore 0\n"
                           "    bloadc_t\n"
                           "    bload_0\n"
                           "    beq\n"
                           "    bstore 0\n"
                           "    floadc 3\n"
                           "    floadc 4\n"
                           "    fmul\n"
                           "    floadc 5\n"
                           "    fneg\n"
                           "    floadc 4\n"
                           "    fdiv\n"
                           "    flt\n"
                           "    bstore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 6\n"
                           "    idiv\n"
                           "    ile\n"
                           "    bstore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc 2\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    ige\n"
                           "    bstore 0\n"
                           "    bload_0\n"
                           "    bnot\n"
                           "    bstore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 2\n"
                           "    igt\n"
                           "    bnot\n"
                           "    bnot\n"
                           "    bnot\n"
                           "    bnot\n"
                           "    bstore 0\n"
                           "    bload_0\n"
                           "    bload_0\n"
                           "    bload_0\n"
                           "    bmul\n"
                           "    badd\n"
                           "    bstore 0\n"
                           "    iload_1\n"
                           "    iload_1\n"
                           "    irem\n"
                           "    iloadc 0\n"
                           "    ineg\n"
                           "    iadd\n"
                           "    istore 1\n"
                           "    bload_0\n"
                           "    branch_f pfalse1\n"
                           "    bload_0\n"
                           "    jump pend1\n"
                           "pfalse1:\n"
                           "    bloadc_f\n"
                           "pend1:\n"
                           "    branch_f pfalse0\n"
                           "    bloadc_t\n"
                           "    jump pend0\n"
                           "pfalse0:\n"
                           "    bloadc_f\n"
                           "pend0:\n"
                           "    bstore 0\n"
                           "    bload_0\n"
                           "    branch_f pfalse3\n"
                           "    bloadc_t\n"
                           "    jump pend3\n"
                           "pfalse3:\n"
                           "    bload_0\n"
                           "pend3:\n"
                           "    branch_f pfalse2\n"
                           "    bloadc_t\n"
                           "    jump pend2\n"
                           "pfalse2:\n"
                           "    bloadc_f\n"
                           "pend2:\n"
                           "    bstore 0\n"
                           "    return\n"
                           "retint:\n"
                           "    bloadc_t\n"
                           "    branch_f ifend4\n"
                           "    iloadc_1\n"
                           "    ireturn\n"
                           "ifend4:\n"
                           "    iloadc 0\n"
                           "    ireturn\n"
                           "lf0_retfloat:\n"
                           "    bloadc_t\n"
                           "    branch_f else5\n"
                           ".const float 0x1.8p+2  ; 6.000000e+00\n"
                           "    floadc 7\n"
                           "    freturn\n"
                           "    jump ifend5\n"
                           "else5:\n"
                           ".const float 0x1p+2  ; 4.000000e+00\n"
                           "    floadc 8\n"
                           "    freturn\n"
                           "ifend5:\n"
                           "lf1_retbool:\n"
                           "    bloadc_t\n"
                           "    breturn\n"
                           "cast:\n"
                           "    esr 3\n"
                           "    bloadc_t\n"
                           "    branch_f pfalse8\n"
                           "    iloadc_1\n"
                           "    jump pend8\n"
                           "pfalse8:\n"
                           "    iloadc_0\n"
                           "pend8:\n"
                           "    i2f\n"
                           "    floadc_0\n"
                           "    fne\n"
                           "    branch_f pfalse7\n"
                           "    bloadc_t\n"
                           "    jump pend7\n"
                           "pfalse7:\n"
                           "    bloadc_f\n"
                           "pend7:\n"
                           "    branch_f pfalse6\n"
                           "    iloadc_1\n"
                           "    jump pend6\n"
                           "pfalse6:\n"
                           "    iloadc_0\n"
                           "pend6:\n"
                           "    istore 0\n"
                           "    iloadc 0\n"
                           "    iloadc 0\n"
                           "    iadd\n"
                           "    iloadc 2\n"
                           "    iadd\n"
                           ".const int 0x8  ; 8\n"
                           "    iloadc 9\n"
                           "    iadd\n"
                           "    iloadc_0\n"
                           "    ine\n"
                           "    branch_f pfalse9\n"
                           "    bloadc_t\n"
                           "    jump pend9\n"
                           "pfalse9:\n"
                           "    bloadc_f\n"
                           "pend9:\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f pfalse11\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 10\n"
                           "    iload_0\n"
                           "    igt\n"
                           "    jump pend11\n"
                           "pfalse11:\n"
                           "    bloadc_f\n"
                           "pend11:\n"
                           "    branch_f pfalse10\n"
                           "    floadc_1\n"
                           "    jump pend10\n"
                           "pfalse10:\n"
                           "    floadc_0\n"
                           "pend10:\n"
                           "    fstore 2\n"
                           "    return\n"
                           "array:\n"
                           "    esr 20\n"
                           "    iloadc 1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    istore 15\n"
                           "    iloadc 6\n"
                           "    iloadc 0\n"
                           "    imul\n"
                           "    istore 14\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 11\n"
                           "    istore 13\n"
                           "    iloadc_1\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    istore 12\n"
                           "    iloadc 1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    istore 11\n"
                           "    iloadc 11\n"
                           "    istore 8\n"
                           "    iloadc_1\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    istore 5\n"
                           "    iload 5\n"
                           "    bnewa\n"
                           "    astore 22\n"
                           "    bloadc_t\n"
                           "    bstore 3\n"
                           "    iloadc_0\n"
                           "    istore 4\n"
                           "for0:\n"
                           "    iload 4\n"
                           "    iload 5\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           "    bload_3\n"
                           "    iload 4\n"
                           "    aload 22\n"
                           "    bstorea\n"
                           "    iinc_1 4\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    iload 8\n"
                           "    inewa\n"
                           "    astore 21\n"
                           "    iloadc 6\n"
                           "    istore 6\n"
                           "    iloadc_0\n"
                           "    istore 7\n"
                           "for1:\n"
                           "    iload 7\n"
                           "    iload 8\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           "    iload 6\n"
                           "    iload 7\n"
                           "    aload 21\n"
                           "    istorea\n"
                           "    iinc_1 7\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    iload 11\n"
                           "    fnewa\n"
                           "    astore 20\n"
                           "    floadc_1\n"
                           "    fstore 9\n"
                           "    iloadc_0\n"
                           "    istore 10\n"
                           "for2:\n"
                           "    iload 10\n"
                           "    iload 11\n"
                           "    ilt\n"
                           "    branch_f endfor2\n"
                           "    fload 9\n"
                           "    iload 10\n"
                           "    aload 20\n"
                           "    fstorea\n"
                           "    iinc_1 10\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    iload 12\n"
                           "    bnewa\n"
                           "    astore 19\n"
                           "    bloadc_t\n"
                           "    iloadc_0\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 19\n"
                           "    bstorea\n"
                           "    iload 13\n"
                           "    inewa\n"
                           "    astore 18\n"
                           "    iloadc_1\n"
                           "    iloadc_0\n"
                           "    aload 18\n"
                           "    istorea\n"
                           "    iloadc 6\n"
                           "    iloadc_1\n"
                           "    aload 18\n"
                           "    istorea\n"
                           "    iloadc 1\n"
                           "    iloadc 6\n"
                           "    aload 18\n"
                           "    istorea\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 12\n"
                           "    iloadc 1\n"
                           "    aload 18\n"
                           "    istorea\n"
                           "    iloadc 0\n"
                           "    iloadc 12\n"
                           "    aload 18\n"
                           "    istorea\n"
                           "    iloadc 11\n"
                           "    iloadc 0\n"
                           "    aload 18\n"
                           "    istorea\n"
                           "    iload 14\n"
                           "    fnewa\n"
                           "    astore 17\n"
                           "    floadc_1\n"
                           "    iloadc_0\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           ".const float 0x1p+1  ; 2.000000e+00\n"
                           "    floadc 13\n"
                           "    iloadc_0\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           "    floadc 4\n"
                           "    iloadc_0\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc 6\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           "    floadc 8\n"
                           "    iloadc_0\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           "    floadc 3\n"
                           "    iloadc_0\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc 12\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           "    floadc 7\n"
                           "    iloadc_1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           "    floadc 5\n"
                           "    iloadc_1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           ".const float 0x1p+3  ; 8.000000e+00\n"
                           "    floadc 14\n"
                           "    iloadc_1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc 6\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           ".const float 0x1.2p+3  ; 9.000000e+00\n"
                           "    floadc 15\n"
                           "    iloadc_1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           ".const float 0x1.4p+3  ; 1.000000e+01\n"
                           "    floadc 16\n"
                           "    iloadc_1\n"
                           "    iloadc 6\n"
                           "    imul\n"
                           "    iloadc 12\n"
                           "    iadd\n"
                           "    aload 17\n"
                           "    fstorea\n"
                           "    iload 15\n"
                           "    fnewa\n"
                           "    astore 16\n"
                           "    floadc_1\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 16\n"
                           "    fstorea\n"
                           "    floadc 13\n"
                           "    iloadc_0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 16\n"
                           "    fstorea\n"
                           "    floadc 4\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 16\n"
                           "    fstorea\n"
                           "    floadc 8\n"
                           "    iloadc_1\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 16\n"
                           "    fstorea\n"
                           "    floadc 3\n"
                           "    iloadc 6\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_0\n"
                           "    iadd\n"
                           "    aload 16\n"
                           "    fstorea\n"
                           "    floadc 7\n"
                           "    iloadc 6\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload 16\n"
                           "    fstorea\n"
                           "    floadc_1\n"
                           "    iloadc_0\n"
                           "    iload_1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    aload_2\n"
                           "    fstorea\n"
                           "    isrg\n"
                           "    iload_0\n"
                           "    iload_1\n"
                           "    aload_2\n"
                           "    jsr 3 array\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, NestedFor)
{
    SetUp("nested_for/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".exportfun \"test\" void test\n"
                           "test:\n"
                           "    esr 5\n"
                           "    iloadc_0\n"
                           "    istore 1\n"
                           "for0:\n"
                           "    iload_1\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           "    iload_1\n"
                           "    istore 0\n"
                           "    iloadc_0\n"
                           "    istore 2\n"
                           "for1:\n"
                           "    iload_2\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           "    iload_2\n"
                           "    istore 0\n"
                           "    iloadc_0\n"
                           "    istore 3\n"
                           "for2:\n"
                           "    iload_3\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor2\n"
                           "    iload_3\n"
                           "    istore 0\n"
                           "    iloadc_0\n"
                           "    istore 4\n"
                           "for3:\n"
                           "    iload 4\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor3\n"
                           "    iload 4\n"
                           "    istore 0\n"
                           "    iloadc_1\n"
                           "    iinc_1 4\n"
                           "    jump for3\n"
                           "endfor3:\n"
                           "    iload_3\n"
                           "    istore 0\n"
                           "    iloadc_1\n"
                           "    iinc_1 3\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    iload_2\n"
                           "    istore 0\n"
                           "    iloadc_1\n"
                           "    iinc_1 2\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    iload_1\n"
                           "    istore 0\n"
                           "    iloadc_1\n"
                           "    iinc_1 1\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    iloadc_0\n"
                           "    istore 0\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, BadMain)
{
    SetUp("bad_main/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "main:\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, SameIdentifierFunDefVarDecValid)
{
    SetUp("same_identifier/fundef_vardec/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "fun_one:\n"
                           "    iload_0\n"
                           "    ireturn\n"
                           "lf0_param0:\n"
                           "    esr 1\n"
                           "    iloadc_0\n"
                           "    istore 0\n"
                           "    return\n"
                           "fun_two:\n"
                           "    floadc_1\n"
                           "    freturn\n"
                           "lf1_nested:\n"
                           "    esr 2\n"
                           "    iloadc_0\n"
                           "    istore 1\n"
                           "    iloadc_1\n"
                           "    istore 2\n"
                           "    return\n"
                           "fun_three:\n"
                           "    esr 1\n"
                           "    bloadc_t\n"
                           "    breturn\n"
                           "lf2_shadow:\n"
                           "    esr 1\n"
                           "    iloadc_1\n"
                           "    istore 1\n"
                           "    iload_1\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, SameIdentifierParamNestedParam)
{
    SetUp("same_identifier/param_nested_param/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "fun_one:\n"
                           "    iload_0\n"
                           "    ireturn\n"
                           "lf0_nested_fun_one:\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, ProcCallContext)
{
    SetUp("proc_call/context/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "fun:\n"
                           "    iload_0\n"
                           "    ireturn\n"
                           "test:\n"
                           "    esr 1\n"
                           "    isrg\n"
                           "    iloadc_1\n"
                           "    jsr 1 fun\n"
                           "    isrg\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           "    jsr 1 fun\n"
                           "    iadd\n"
                           "    istore 0\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_Binop)
{
    SetUp("testsuite_public/basic/check_success/binops.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    esr 6\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           "    istore 1\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 1\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 2\n"
                           "    iadd\n"
                           "    istore 2\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 3\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 4\n"
                           "    imul\n"
                           "    istore 3\n"
                           ".const int 0x7  ; 7\n"
                           "    iloadc 5\n"
                           ".const int 0x8  ; 8\n"
                           "    iloadc 6\n"
                           "    irem\n"
                           "    istore 4\n"
                           "    iload_2\n"
                           "    iload_3\n"
                           "    iadd\n"
                           "    iload 4\n"
                           "    iload_1\n"
                           "    imul\n"
                           "    isub\n"
                           "    istore 5\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_Boolop)
{
    SetUp("testsuite_public/basic/check_success/boolop.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "f:\n"
                           "    esr 23\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 1\n"
                           "    iadd\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 2\n"
                           "    iadd\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 3\n"
                           "    iadd\n"
                           "    istore 0\n"
                           ".const float 0x1.38p+5  ; 3.900000e+01\n"
                           "    floadc 4\n"
                           ".const float 0x1.8p+1  ; 3.000000e+00\n"
                           "    floadc 5\n"
                           "    fadd\n"
                           ".const float 0x1.5p+4  ; 2.100000e+01\n"
                           "    floadc 6\n"
                           "    fadd\n"
                           "    fstore 1\n"
                           "    bloadc_f\n"
                           "    bloadc_f\n"
                           "    badd\n"
                           "    bstore 2\n"
                           "    iloadc 0\n"
                           "    iloadc 1\n"
                           "    isub\n"
                           "    iloadc 2\n"
                           "    isub\n"
                           "    iloadc 3\n"
                           "    isub\n"
                           "    istore 3\n"
                           "    floadc 4\n"
                           "    floadc 5\n"
                           "    fsub\n"
                           "    floadc 6\n"
                           "    fsub\n"
                           "    fstore 4\n"
                           "    iloadc 0\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    iloadc_1\n"
                           "    imul\n"
                           "    istore 5\n"
                           ".const float 0x1.91eb851eb851fp+1  ; 3.140000e+00\n"
                           "    floadc 7\n"
                           ".const float 0x1p+1  ; 2.000000e+00\n"
                           "    floadc 8\n"
                           "    fmul\n"
                           "    fstore 6\n"
                           "    bloadc_t\n"
                           "    bloadc_f\n"
                           "    bmul\n"
                           "    bstore 7\n"
                           "    iloadc 2\n"
                           "    iloadc 1\n"
                           "    idiv\n"
                           "    istore 8\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 9\n"
                           "    iloadc_0\n"
                           "    idiv\n"
                           "    istore 9\n"
                           ".const float 0x1.4p+2  ; 5.000000e+00\n"
                           "    floadc 10\n"
                           "    floadc 8\n"
                           "    fdiv\n"
                           "    fstore 10\n"
                           ".const float 0x1.2p+3  ; 9.000000e+00\n"
                           "    floadc 11\n"
                           "    floadc_0\n"
                           "    fdiv\n"
                           "    fstore 11\n"
                           ".const int 0xc  ; 12\n"
                           "    iloadc 12\n"
                           ".const int 0x8  ; 8\n"
                           "    iloadc 13\n"
                           "    irem\n"
                           "    iloadc 1\n"
                           "    irem\n"
                           "    istore 12\n"
                           ".const int 0x14  ; 20\n"
                           "    iloadc 14\n"
                           "    iloadc 14\n"
                           "    ilt\n"
                           "    bstore 13\n"
                           "    floadc 5\n"
                           ".const float 0x1.8666666666666p+2  ; 6.100000e+00\n"
                           "    floadc 15\n"
                           "    flt\n"
                           "    bstore 14\n"
                           "    iloadc 14\n"
                           "    iloadc 14\n"
                           "    ile\n"
                           "    bstore 15\n"
                           "    floadc 5\n"
                           "    floadc 15\n"
                           "    fle\n"
                           "    bstore 16\n"
                           ".const int 0x3c  ; 60\n"
                           "    iloadc 16\n"
                           ".const int 0x32  ; 50\n"
                           "    iloadc 17\n"
                           "    ilt\n"
                           "    branch_f pfalse0\n"
                           "    bloadc_t\n"
                           "    jump pend0\n"
                           "pfalse0:\n"
                           ".const int 0x1e  ; 30\n"
                           "    iloadc 18\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 19\n"
                           "    igt\n"
                           "pend0:\n"
                           "    bstore 17\n"
                           "    floadc 5\n"
                           "    floadc 8\n"
                           "    fgt\n"
                           "    bstore 18\n"
                           "    iloadc 13\n"
                           "    iloadc 13\n"
                           "    ine\n"
                           "    bstore 19\n"
                           "    iloadc 13\n"
                           "    iloadc 13\n"
                           "    ieq\n"
                           "    bstore 20\n"
                           ".const float 0x1.3333333333333p+0  ; 1.200000e+00\n"
                           "    floadc 20\n"
                           ".const float 0x1.0cccccccccccdp+1  ; 2.100000e+00\n"
                           "    floadc 21\n"
                           "    fne\n"
                           "    bstore 21\n"
                           "    bloadc_t\n"
                           "    bloadc_f\n"
                           "    bne\n"
                           "    bstore 22\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);

    ASSERT_THAT(err_output, testing::HasSubstr("warning: Division by zero"));
}

TEST_F(GenerationTest, Suite_Basic_CommentMultiline)
{
    SetUp("testsuite_public/basic/check_success/comment_multiline.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".global int\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_CommentSingleline)
{
    SetUp("testsuite_public/basic/check_success/comment_singleline.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".global int\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_DoWhile)
{
    SetUp("testsuite_public/basic/check_success/do_while.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "test_do_while2:\n"
                           "    esr 2\n"
                           "    iloadc_0\n"
                           "    istore 0\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 0\n"
                           "    istore 1\n"
                           "while0:\n"
                           "    iinc_1 0\n"
                           "    idec_1 1\n"
                           "    iload_0\n"
                           "    iload_1\n"
                           "    ile\n"
                           "    branch_t while0\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_EarlyReturn)
{
    SetUp("testsuite_public/basic/check_success/early_return.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    esr 1\n"
                           "    iloadc_1\n"
                           "    istore 1\n"
                           "    iload_0\n"
                           "    iloadc_1\n"
                           "    igt\n"
                           "    branch_f ifend0\n"
                           "    return\n"
                           "ifend0:\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           "    istore 1\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ExternVar)
{
    SetUp("testsuite_public/basic/check_success/extern_var.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importvar \"pi\" float\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_NoReturn)
{
    SetUp("testsuite_public/basic/check_success/no_return.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "baz:\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParamsFuncall)
{
    SetUp("testsuite_public/basic/check_success/params_funcall.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    ipop\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParamsSimple)
{
    SetUp("testsuite_public/basic/check_success/params_simple.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "func:\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseAssign)
{
    SetUp("testsuite_public/basic/check_success/parse_assign.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".global int\n"
                           "foo:\n"
                           "    esr 1\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    iloadc_1\n"
                           "    istoreg 0\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseDoWhile)
{
    SetUp("testsuite_public/basic/check_success/parse_do_while.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "while0:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    bloadc_t\n"
                           "    branch_t while0\n"
                           "while1:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    bloadc_f\n"
                           "    branch_t while1\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseFor)
{
    SetUp("testsuite_public/basic/check_success/parse_for.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    esr 5\n"
                           "    iloadc_0\n"
                           "    istore 0\n"
                           "for0:\n"
                           "    iload_0\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    iinc_1 0\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    iloadc_0\n"
                           "    istore 1\n"
                           "for1:\n"
                           "    iload_1\n"
                           "    iloadc 0\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    iinc_1 1\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    iloadc_0\n"
                           "    istore 3\n"
                           "    iloadc_1\n"
                           "    ineg\n"
                           "    istore 4\n"
                           "    iloadc 0\n"
                           "    istore 2\n"
                           "    iload_3\n"
                           "    iload_2\n"
                           "    isub\n"
                           "    iload 4\n"
                           "    idiv\n"
                           "    istore 3\n"
                           "for2:\n"
                           "    iload 4\n"
                           "    iloadc_0\n"
                           "    igt\n"
                           "    branch_f endfor2\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    idec_1 3\n"
                           "    iload_2\n"
                           "    iload 4\n"
                           "    iadd\n"
                           "    istore 4\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseFunbody)
{
    SetUp("testsuite_public/basic/check_success/parse_funbody.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "vardec:\n"
                           "    esr 1\n"
                           "    return\n"
                           "vardec_stat:\n"
                           "    esr 1\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    return\n"
                           "vardec_ret:\n"
                           "    esr 1\n"
                           "    iloadc 0\n"
                           "    ireturn\n"
                           "vardec_stat_ret:\n"
                           "    esr 1\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    ireturn\n"
                           "stat_ret:\n"
                           "    isrg\n"
                           "    jsr 0 vardec\n"
                           "    iloadc 0\n"
                           "    ireturn\n"
                           "ret:\n"
                           "    iloadc 0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseFuncall)
{
    SetUp("testsuite_public/basic/check_success/parse_funcall.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseGlobaldec)
{
    SetUp("testsuite_public/basic/check_success/parse_globaldec.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importvar \"a\" bool\n"
                           ".importvar \"b\" int\n"
                           ".importvar \"c\" float\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseGlobaldef)
{
    SetUp("testsuite_public/basic/check_success/parse_globaldef.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".global int\n"
                           ".global float\n"
                           ".global bool\n"
                           ".global int\n"
                           ".global float\n"
                           ".global bool\n"
                           ".global int\n"
                           ".exportvar \"a2\" 6\n"
                           ".global float\n"
                           ".exportvar \"b2\" 7\n"
                           ".global bool\n"
                           ".exportvar \"c2\" 8\n"
                           ".global int\n"
                           ".exportvar \"d2\" 9\n"
                           ".global float\n"
                           ".exportvar \"e2\" 10\n"
                           ".global bool\n"
                           ".exportvar \"f2\" 11\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    bloadc_f\n"
                           "    bstoreg 11\n"
                           "    floadc_1\n"
                           "    fstoreg 10\n"
                           ".const int 0x141  ; 321\n"
                           "    iloadc 0\n"
                           "    istoreg 9\n"
                           "    bloadc_f\n"
                           "    bstoreg 5\n"
                           "    floadc_1\n"
                           "    fstoreg 4\n"
                           "    iloadc 0\n"
                           "    istoreg 3\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseIfElse)
{
    SetUp("testsuite_public/basic/check_success/parse_if_else.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    bloadc_t\n"
                           "    branch_f else0\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    jump ifend0\n"
                           "else0:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "ifend0:\n"
                           "    bloadc_f\n"
                           "    branch_f else1\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    jump ifend1\n"
                           "else1:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "ifend1:\n"
                           "    bloadc_t\n"
                           "    branch_f else2\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    jump ifend2\n"
                           "else2:\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "ifend2:\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_IntegerOutOfRange)
{
    SetUp("testsuite_public/basic/check_error/integer_out_of_range.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".global int\n"
                           ".importfun \"printInt\" void int\n"
                           ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    isrg\n"
                           "    iloadg 0\n"
                           "    jsre 0\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           ".const int 0x7fffffff  ; 2147483647\n"
                           "    iloadc 0\n"
                           "    istoreg 0\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseOperators)
{
    SetUp("testsuite_public/basic/check_success/parse_operators.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    esr 2\n"
                           "    iloadc_1\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 0\n"
                           "    iadd\n"
                           "    istore 0\n"
                           "    idec_1 0\n"
                           "    iload_0\n"
                           ".const int 0x2d  ; 45\n"
                           "    iloadc 1\n"
                           "    imul\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           ".const int 0x9  ; 9\n"
                           "    iloadc 2\n"
                           "    idiv\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 3\n"
                           "    irem\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    iloadc_1\n"
                           "    ilt\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    iloadc 0\n"
                           "    igt\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 4\n"
                           "    ile\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    iloadc 3\n"
                           "    ige\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 5\n"
                           "    ieq\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 6\n"
                           "    ine\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    iloadc 6\n"
                           "    ine\n"
                           "    branch_f pfalse0\n"
                           "    iload_0\n"
                           "    iloadc 3\n"
                           "    ige\n"
                           "    jump pend0\n"
                           "pfalse0:\n"
                           "    bloadc_f\n"
                           "pend0:\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    iloadc 6\n"
                           "    ieq\n"
                           "    branch_f pfalse1\n"
                           "    bloadc_t\n"
                           "    jump pend1\n"
                           "pfalse1:\n"
                           "    iload_0\n"
                           "    iloadc 3\n"
                           "    ile\n"
                           "pend1:\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    ineg\n"
                           "    istore 0\n"
                           "    bload_1\n"
                           "    bnot\n"
                           "    bstore 1\n"
                           "    iload_0\n"
                           "    iloadc_1\n"
                           "    ineg\n"
                           "    isub\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    ineg\n"
                           "    ineg\n"
                           "    ineg\n"
                           "    ineg\n"
                           "    ineg\n"
                           "    ineg\n"
                           "    istore 0\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseTypecast)
{
    SetUp("testsuite_public/basic/check_success/parse_typecast.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    esr 3\n"
                           "    floadc_1\n"
                           "    f2i\n"
                           "    istore 0\n"
                           "    iloadc_1\n"
                           "    i2f\n"
                           "    fstore 1\n"
                           "    iloadc_1\n"
                           "    iloadc_0\n"
                           "    ine\n"
                           "    branch_f pfalse0\n"
                           "    bloadc_t\n"
                           "    jump pend0\n"
                           "pfalse0:\n"
                           "    bloadc_f\n"
                           "pend0:\n"
                           "    bstore 2\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_ParseVardec)
{
    SetUp("testsuite_public/basic/check_success/parse_vardec.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "foo:\n"
                           "    esr 6\n"
                           "    bloadc_t\n"
                           "    bstore 3\n"
                           "    iloadc_1\n"
                           "    istore 4\n"
                           "    floadc_1\n"
                           "    fstore 5\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Basic_VardecInit)
{
    SetUp("testsuite_public/basic/check_success/vardec_init.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "testVarInits:\n"
                           "    esr 4\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 1\n"
                           "    istore 1\n"
                           "    iload_0\n"
                           "    iload_1\n"
                           "    iadd\n"
                           "    istore 2\n"
                           "    iload_0\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 2\n"
                           "    iadd\n"
                           "    istore 3\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Arrays_ExternArrayArg)
{
    SetUp("testsuite_public/arrays/check_success/extern_array_arg.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importvar \"__dim0_arr\" int\n"
                           ".importvar \"arr\" int[]\n"
                           "foo:\n"
                           "    return\n"
                           ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    isrg\n"
                           "    iloade 0\n"
                           "    aloade 1\n"
                           "    jsr 2 foo\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Arrays_LocalArraydef)
{
    SetUp("testsuite_public/arrays/check_success/local_arraydef.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "func:\n"
                           "    esr 6\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 0\n"
                           "    istore 2\n"
                           ".const int 0x14  ; 20\n"
                           "    iloadc 1\n"
                           ".const int 0x1e  ; 30\n"
                           "    iloadc 2\n"
                           "    imul\n"
                           "    istore 1\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 3\n"
                           ".const int 0x5  ; 5\n"
                           "    iloadc 4\n"
                           "    imul\n"
                           ".const int 0x6  ; 6\n"
                           "    iloadc 5\n"
                           "    imul\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    inewa\n"
                           "    astore 5\n"
                           "    iload_1\n"
                           "    inewa\n"
                           "    astore 4\n"
                           "    iload_2\n"
                           "    inewa\n"
                           "    astore 3\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Arrays_ScanVectorMatrix)
{
    SetUp("testsuite_public/arrays/check_success/scan_vector_matrix.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importfun \"scanInt\" int\n"
                           ".importfun \"scanFloat\" float\n"
                           ".importfun \"printInt\" void int\n"
                           ".importfun \"printFloat\" void float\n"
                           "scan_vector:\n"
                           "    esr 2\n"
                           "    iload_0\n"
                           "    istore 3\n"
                           "    iloadc_0\n"
                           "    istore 2\n"
                           "for0:\n"
                           "    iload_2\n"
                           "    iload_3\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           "    isrg\n"
                           "    jsre 1\n"
                           "    iload_2\n"
                           "    aload_1\n"
                           "    fstorea\n"
                           "    iinc_1 2\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    return\n"
                           "scan_matrix:\n"
                           "    esr 4\n"
                           "    iload_1\n"
                           "    istore 5\n"
                           "    iloadc_0\n"
                           "    istore 3\n"
                           "for1:\n"
                           "    iload_3\n"
                           "    iload 5\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           "    iload_0\n"
                           "    istore 6\n"
                           "    iloadc_0\n"
                           "    istore 4\n"
                           "for2:\n"
                           "    iload 4\n"
                           "    iload 6\n"
                           "    ilt\n"
                           "    branch_f endfor2\n"
                           "    isrg\n"
                           "    jsre 1\n"
                           "    iload_3\n"
                           "    iload_1\n"
                           "    imul\n"
                           "    iload 4\n"
                           "    iadd\n"
                           "    aload_2\n"
                           "    fstorea\n"
                           "    iinc_1 4\n"
                           "    jump for2\n"
                           "endfor2:\n"
                           "    iinc_1 3\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    return\n"
                           ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    esr 2\n"
                           ".const int 0x3  ; 3\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    iloadc 0\n"
                           "    istore 1\n"
                           "    iload_1\n"
                           "    iloadc_1\n"
                           "    ieq\n"
                           "    branch_f pfalse1\n"
                           "    iload_0\n"
                           "    iloadc_1\n"
                           "    ige\n"
                           "    jump pend1\n"
                           "pfalse1:\n"
                           "    bloadc_f\n"
                           "pend1:\n"
                           "    branch_f else0\n"
                           "    isrl\n"
                           "    jsr 0 lf0_do_vector\n"
                           "    jump ifend0\n"
                           "else0:\n"
                           "    iload_1\n"
                           "    iloadc_1\n"
                           "    igt\n"
                           "    branch_f pfalse3\n"
                           "    iload_0\n"
                           "    iloadc_1\n"
                           "    ige\n"
                           "    jump pend3\n"
                           "pfalse3:\n"
                           "    bloadc_f\n"
                           "pend3:\n"
                           "    branch_f ifend2\n"
                           "    isrl\n"
                           "    jsr 0 lf1_do_matrix\n"
                           "ifend2:\n"
                           "ifend0:\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           "lf0_do_vector:\n"
                           "    esr 2\n"
                           "    iloadn 1 0\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    fnewa\n"
                           "    astore 1\n"
                           "    isrg\n"
                           "    iloadn 1 0\n"
                           "    aload_1\n"
                           "    jsr 2 scan_vector\n"
                           "    return\n"
                           "lf1_do_matrix:\n"
                           "    esr 2\n"
                           "    iloadn 1 0\n"
                           "    iloadn 1 1\n"
                           "    imul\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    fnewa\n"
                           "    astore 1\n"
                           "    isrg\n"
                           "    iloadn 1 1\n"
                           "    iloadn 1 0\n"
                           "    aload_1\n"
                           "    jsr 3 scan_matrix\n"
                           "    return\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}

TEST_F(GenerationTest, Suite_Arrays_Scopes)
{
    SetUp("testsuite_public/arrays/check_success/scopes.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = ".importfun \"printInt\" void int\n"
                           ".importfun \"printNewlines\" void int\n"
                           ".global int\n"
                           ".global int\n"
                           ".global int[]\n"
                           ".importvar \"__dim0_d\" int\n"
                           ".importvar \"d\" int[]\n"
                           "foo:\n"
                           "    esr 3\n"
                           "    iload_0\n"
                           "    iloadg 1\n"
                           "    iadd\n"
                           "    iloadc_1\n"
                           "    iadd\n"
                           "    istore 0\n"
                           "    iload_1\n"
                           "    istore 1\n"
                           "    iinc_1 0\n"
                           "    iload_1\n"
                           "    iloadc_1\n"
                           "    isub\n"
                           "    istore 2\n"
                           "    isrl\n"
                           ".const int 0x4  ; 4\n"
                           "    iloadc 0\n"
                           "    iload_0\n"
                           "    iload_2\n"
                           "    aloade 1\n"
                           "    iloada\n"
                           "    iadd\n"
                           "    aloadg 2\n"
                           "    jsr 3 lf0_baz\n"
                           "    ireturn\n"
                           "lf0_baz:\n"
                           "    esr 1\n"
                           "    iload_1\n"
                           "    iload_0\n"
                           "    iadd\n"
                           "    istore 3\n"
                           "    iload_3\n"
                           "    iload_1\n"
                           "    aload_2\n"
                           "    iloada\n"
                           "    iadd\n"
                           "    ireturn\n"
                           "bar:\n"
                           "    esr 2\n"
                           "    iloadg 1\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    ireturn\n"
                           "baz:\n"
                           "    esr 3\n"
                           "    isrg\n"
                           "    iload_0\n"
                           "    jsre 0\n"
                           "    iloadc_1\n"
                           "    istore 1\n"
                           "for0:\n"
                           "    iload_1\n"
                           ".const int 0xa  ; 10\n"
                           "    iloadc 1\n"
                           "    ilt\n"
                           "    branch_f endfor0\n"
                           "    isrg\n"
                           "    iload_1\n"
                           "    jsre 0\n"
                           "    iloadc_1\n"
                           "    istore 2\n"
                           "for1:\n"
                           "    iload_2\n"
                           "    iloadc 1\n"
                           "    ilt\n"
                           "    branch_f endfor1\n"
                           "    isrg\n"
                           "    iload_2\n"
                           "    jsre 0\n"
                           "    iinc_1 2\n"
                           "    jump for1\n"
                           "endfor1:\n"
                           "    isrg\n"
                           "    iload_1\n"
                           "    jsre 0\n"
                           "    iinc_1 1\n"
                           "    jump for0\n"
                           "endfor0:\n"
                           "    isrg\n"
                           "    iload_0\n"
                           "    jsre 0\n"
                           "    return\n"
                           ".exportfun \"main\" int main\n"
                           "main:\n"
                           "    isrg\n"
                           "    isrg\n"
                           "    jsr 0 foo\n"
                           "    jsre 0\n"
                           "    isrg\n"
                           "    iloadc_1\n"
                           "    jsre 1\n"
                           "    isrg\n"
                           "    isrg\n"
                           "    jsr 0 bar\n"
                           "    jsre 0\n"
                           "    isrg\n"
                           "    iloadc_1\n"
                           "    jsre 1\n"
                           "    isrg\n"
                           "    jsr 0 baz\n"
                           "    isrg\n"
                           "    iloadc_1\n"
                           "    jsre 1\n"
                           "    iloadc_0\n"
                           "    ireturn\n"
                           ".exportfun \"__init\" void __init\n"
                           "__init:\n"
                           "    esr 1\n"
                           "    iloadc 0\n"
                           "    istore 0\n"
                           "    iload_0\n"
                           "    inewa\n"
                           "    astoreg 2\n"
                           ".const int 0x2  ; 2\n"
                           "    iloadc 2\n"
                           "    istoreg 1\n"
                           "    iloadc_1\n"
                           "    istoreg 0\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}
