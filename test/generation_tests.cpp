#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <filesystem>
#include <iostream>
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
    node_st *root = nullptr;
    std::string input_filepath;
    std::string err_output;
    std::string std_output;
    const int output_buffer_size = 16 * 1024;
    // We do not want to put large memory allocation on the stack, also we want to use the benefits
    // of ASan.
    char *output_buffer = nullptr;

  protected:
    GenerationTest()
    {
        output_buffer = new char[output_buffer_size];
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

        if (root != nullptr)
        {
            cleanup_nodes(root);
            root = nullptr;
        }

        if (output_buffer != nullptr)
        {
            delete[] output_buffer;
        }
    }
};
