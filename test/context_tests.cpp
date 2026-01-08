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
    char *symbols_string = nullptr;
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
    }
};

TEST_F(ContextTest, NestedFor)
{
    SetUp("nested_for/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'1'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'test'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@for0_i'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@for1_i'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@for2_i'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@for3_i'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ ForLoop\n"
                           "┃        ┃     ├─ Assign\n"
                           "┃        ┃     │  ├─ Var -- name:'@for0_i'\n"
                           "┃        ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ├─ Int -- val:'5'\n"
                           "┃        ┃     ├─ Int -- val:'1'\n"
                           "┃        ┃     ┢─ Statements\n"
                           "┃        ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     └─ Var -- name:'@for0_i'\n"
                           "┃        ┃     ┣─ Statements\n"
                           "┃        ┃     ┃  └─ ForLoop\n"
                           "┃        ┃     ┃     ├─ Assign\n"
                           "┃        ┃     ┃     │  ├─ Var -- name:'@for1_i'\n"
                           "┃        ┃     ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ┃     ├─ Int -- val:'5'\n"
                           "┃        ┃     ┃     ├─ Int -- val:'1'\n"
                           "┃        ┃     ┃     ┢─ Statements\n"
                           "┃        ┃     ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     ┃     └─ Var -- name:'@for1_i'\n"
                           "┃        ┃     ┃     ┣─ Statements\n"
                           "┃        ┃     ┃     ┃  └─ ForLoop\n"
                           "┃        ┃     ┃     ┃     ├─ Assign\n"
                           "┃        ┃     ┃     ┃     │  ├─ Var -- name:'@for2_i'\n"
                           "┃        ┃     ┃     ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ┃     ┃     ├─ Int -- val:'5'\n"
                           "┃        ┃     ┃     ┃     ├─ Int -- val:'1'\n"
                           "┃        ┃     ┃     ┃     ┢─ Statements\n"
                           "┃        ┃     ┃     ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     ┃     ┃     └─ Var -- name:'@for2_i'\n"
                           "┃        ┃     ┃     ┃     ┣─ Statements\n"
                           "┃        ┃     ┃     ┃     ┃  └─ ForLoop\n"
                           "┃        ┃     ┃     ┃     ┃     ├─ Assign\n"
                           "┃        ┃     ┃     ┃     ┃     │  ├─ Var -- name:'@for3_i'\n"
                           "┃        ┃     ┃     ┃     ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ┃     ┃     ┃     ├─ Int -- val:'5'\n"
                           "┃        ┃     ┃     ┃     ┃     ├─ Int -- val:'1'\n"
                           "┃        ┃     ┃     ┃     ┃     ┢─ Statements\n"
                           "┃        ┃     ┃     ┃     ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ┃     ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     ┃     ┃     ┃     └─ Var -- name:'@for3_i'\n"
                           "┃        ┃     ┃     ┃     ┃     ┗─ NULL\n"
                           "┃        ┃     ┃     ┃     ┣─ Statements\n"
                           "┃        ┃     ┃     ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     ┃     ┃     └─ Var -- name:'@for2_i'\n"
                           "┃        ┃     ┃     ┃     ┗─ NULL\n"
                           "┃        ┃     ┃     ┣─ Statements\n"
                           "┃        ┃     ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     ┃     └─ Var -- name:'@for1_i'\n"
                           "┃        ┃     ┃     ┗─ NULL\n"
                           "┃        ┃     ┣─ Statements\n"
                           "┃        ┃     ┃  └─ Assign\n"
                           "┃        ┃     ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     ┃     └─ Var -- name:'@for0_i'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program \n"
               "├─ test: FunHeader -- type:'void' -- Params: (null)\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef 'test' -- parent: '0: Program'\n"
               "├─ @for1_i: VarDec -- type:'int'\n"
               "├─ b: VarDec -- type:'int'\n"
               "├─ @for2_i: VarDec -- type:'int'\n"
               "├─ @for3_i: VarDec -- type:'int'\n"
               "├─ @for0_i: VarDec -- type:'int'\n"
               "└────────────────────\n";
    ASSERT_MLSTREQ(expected, symbols_string);
}

TEST_F(ContextTest, BadMain)
{
    SetUp("bad_main/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'main'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'a'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'a'\n"
                           "┃        ┃     └─ Int -- val:'5'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program \n"
               "├─ main: FunHeader -- type:'void' -- Params: int (Var -- name:'a'), \n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef 'main' -- parent: '0: Program'\n"
               "├─ a: Params -- type:'int'\n"
               "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);

    ASSERT_THAT(
        err_output,
        testing::HasSubstr("warning: Defined main functions is missing the 'export' attribute."));
    ASSERT_THAT(
        err_output,
        testing::HasSubstr(
            "warning: Defined main function should be of type 'int' to return an exit code."));
    ASSERT_THAT(err_output,
                testing::HasSubstr(
                    "warning: Defined main function should not contain any function parameters."));
}

// /////////////////////////
// COMPILATION FAILURE tests
// /////////////////////////

TEST_F(ContextTest, GlobalDecInt)
{
    SetUpNoExecute("globaldec_int/main.cvc");
    ASSERT_EXIT(
        run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
        testing::AllOf(testing::HasSubstr("was not declared"), testing::HasSubstr("6 Error")));
}
