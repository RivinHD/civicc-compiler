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
                           "    esr 0\n"
                           "    return\n";

    ASSERT_MLSTREQ(expected, output_buffer);
}
