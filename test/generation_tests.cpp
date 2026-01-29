#include "testutils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <iostream>
#include <stdint.h>
#include <string>

extern "C"
{
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

    const char *expected = "";

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
