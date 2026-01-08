#include "testutils.h"
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

class ContextTest : public testing::Test
{
  public:
    char *root_string = nullptr;
    node_st *root = nullptr;
    std::string input_filepath;
    std::string err_output;
    std::string std_output;

  protected:
    ContextTest()
    {
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
        root = run_context_analysis(input_filepath.c_str());
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
    }
};

TEST_F(ContextTest, GlobalDecInt)
{
    SetUpNoExecute("globaldec_int/main.cvc");
    ASSERT_EXIT(
        run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
        testing::AllOf(testing::HasSubstr("was not declared"), testing::HasSubstr("6 Error")));
}
