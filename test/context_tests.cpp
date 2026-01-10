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
                           "┃     │  ├─ Var -- name:'test_void'\n"
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
               "├─ test_void: FunHeader -- type:'void' -- Params: (null)\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef 'test_void' -- parent: '0: Program'\n"
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

TEST_F(ContextTest, DoubleFundefValid)
{
    SetUp("double/fundef_valid/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'fun_one_int_int'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'test'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'i'\n"
                           "┃        ┃     └─ Int -- val:'1'\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Var -- name:'i'\n"
                           "┃        ┗─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'fun_one_int_int_int'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'test'\n"
                           "┃     │  ┣─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'test_2'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'o'\n"
                           "┃        ┃     └─ Int -- val:'1'\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'o'\n"
                           "┃        ┃     └─ Var -- name:'test'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Var -- name:'test'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program \n"
               "├─ fun_one_int_int: FunHeader -- type:'int' -- Params: int (Var -- name:'test'), \n"
               "├─ fun_one_int_int_int: FunHeader -- type:'int' -- Params: int (Var -- "
               "name:'test'), int (Var -- name:'test_2'), \n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef 'fun_one_int_int' -- parent: '0: Program'\n"
               "├─ i: VarDec -- type:'int'\n"
               "├─ test: Params -- type:'int'\n"
               "└────────────────────\n"
               "┌─ 1: FunDef 'fun_one_int_int_int' -- parent: '0: Program'\n"
               "├─ o: VarDec -- type:'int'\n"
               "├─ test_2: Params -- type:'int'\n"
               "├─ test: Params -- type:'int'\n"
               "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);
}

TEST_F(ContextTest, SameIdentifierFunDefVarDecValid)
{
    SetUp("same_identifier/fundef_vardec/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'fun_one_int_int'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param0'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'void'\n"
                           "┃        ┃     │  ├─ Var -- name:'param0_void'\n"
                           "┃        ┃     │  └─ NULL\n"
                           "┃        ┃     └─ FunBody\n"
                           "┃        ┃        ┢─ VarDecs\n"
                           "┃        ┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃        ┃     ├─ Var -- name:'a'\n"
                           "┃        ┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┃        ┡─ NULL\n"
                           "┃        ┃        ├─ NULL\n"
                           "┃        ┃        └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Var -- name:'param0'\n"
                           "┃        ┗─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'float'\n"
                           "┃     │  ├─ Var -- name:'fun_two_float_int'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param1'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'void'\n"
                           "┃        ┃     │  ├─ Var -- name:'nested_void_int'\n"
                           "┃        ┃     │  ┢─ Params -- type:'int'\n"
                           "┃        ┃     │  ┃  └─ Var -- name:'val1'\n"
                           "┃        ┃     │  ┗─ NULL\n"
                           "┃        ┃     └─ FunBody\n"
                           "┃        ┃        ┢─ VarDecs\n"
                           "┃        ┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃        ┃     ├─ Var -- name:'a'\n"
                           "┃        ┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┃        ┣─ VarDecs\n"
                           "┃        ┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃        ┃     ├─ Var -- name:'nested'\n"
                           "┃        ┃        ┃     └─ Int -- val:'1'\n"
                           "┃        ┃        ┡─ NULL\n"
                           "┃        ┃        ├─ NULL\n"
                           "┃        ┃        └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Float -- val:'1.000000'\n"
                           "┃        ┗─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'bool'\n"
                           "┃     │  ├─ Var -- name:'fun_three_bool_int'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param2'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'shadow'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'int'\n"
                           "┃        ┃     │  ├─ Var -- name:'shadow_int_int'\n"
                           "┃        ┃     │  ┢─ Params -- type:'int'\n"
                           "┃        ┃     │  ┃  └─ Var -- name:'val1'\n"
                           "┃        ┃     │  ┗─ NULL\n"
                           "┃        ┃     └─ FunBody\n"
                           "┃        ┃        ┢─ VarDecs\n"
                           "┃        ┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃        ┃     ├─ Var -- name:'a'\n"
                           "┃        ┃        ┃     └─ Int -- val:'1'\n"
                           "┃        ┃        ┡─ NULL\n"
                           "┃        ┃        ├─ NULL\n"
                           "┃        ┃        ┢─ Statements\n"
                           "┃        ┃        ┃  └─ RetStatement\n"
                           "┃        ┃        ┃     └─ Var -- name:'a'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Bool -- val:'1'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected =
        "┌─ 0: Program \n"
        "├─ fun_two_float_int: FunHeader -- type:'float' -- Params: int (Var -- name:'param1'), \n"
        "├─ fun_one_int_int: FunHeader -- type:'int' -- Params: int (Var -- name:'param0'), \n"
        "├─ fun_three_bool_int: FunHeader -- type:'bool' -- Params: int (Var -- name:'param2'), \n"
        "└────────────────────\n"
        "\n"
        "┌─ 1: FunDef 'fun_one_int_int' -- parent: '0: Program'\n"
        "├─ param0: Params -- type:'int'\n"
        "├─ param0_void: FunHeader -- type:'void' -- Params: (null)\n"
        "└────────────────────\n"
        "┌─ 2: FunDef 'param0_void' -- parent: '1: FunDef 'fun_one_int_int''\n"
        "├─ a: VarDec -- type:'int'\n"
        "└────────────────────\n"
        "┌─ 1: FunDef 'fun_two_float_int' -- parent: '0: Program'\n"
        "├─ param1: Params -- type:'int'\n"
        "├─ nested_void_int: FunHeader -- type:'void' -- Params: int (Var -- name:'val1'), \n"
        "└────────────────────\n"
        "┌─ 2: FunDef 'nested_void_int' -- parent: '1: FunDef 'fun_two_float_int''\n"
        "├─ a: VarDec -- type:'int'\n"
        "├─ val1: Params -- type:'int'\n"
        "├─ nested: VarDec -- type:'int'\n"
        "└────────────────────\n"
        "┌─ 1: FunDef 'fun_three_bool_int' -- parent: '0: Program'\n"
        "├─ param2: Params -- type:'int'\n"
        "├─ shadow: VarDec -- type:'int'\n"
        "├─ shadow_int_int: FunHeader -- type:'int' -- Params: int (Var -- name:'val1'), \n"
        "└────────────────────\n"
        "┌─ 2: FunDef 'shadow_int_int' -- parent: '1: FunDef 'fun_three_bool_int''\n"
        "├─ a: VarDec -- type:'int'\n"
        "├─ val1: Params -- type:'int'\n"
        "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);
}

TEST_F(ContextTest, SameIdentifierParamNestedParam)
{
    SetUp("same_identifier/param_nested_param/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'fun_one_int_int'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param0'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'void'\n"
                           "┃        ┃     │  ├─ Var -- name:'nested_fun_one_void_int'\n"
                           "┃        ┃     │  ┢─ Params -- type:'int'\n"
                           "┃        ┃     │  ┃  └─ Var -- name:'param0'\n"
                           "┃        ┃     │  ┗─ NULL\n"
                           "┃        ┃     └─ FunBody\n"
                           "┃        ┃        ├─ NULL\n"
                           "┃        ┃        ├─ NULL\n"
                           "┃        ┃        └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Var -- name:'param0'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected =
        "┌─ 0: Program \n"
        "├─ fun_one_int_int: FunHeader -- type:'int' -- Params: int (Var -- name:'param0'), \n"
        "└────────────────────\n"
        "\n"
        "┌─ 1: FunDef 'fun_one_int_int' -- parent: '0: Program'\n"
        "├─ param0: Params -- type:'int'\n"
        "├─ nested_fun_one_void_int: FunHeader -- type:'void' -- Params: int (Var -- "
        "name:'param0'), \n"
        "└────────────────────\n"
        "┌─ 2: FunDef 'nested_fun_one_void_int' -- parent: '1: FunDef 'fun_one_int_int''\n"
        "├─ param0: Params -- type:'int'\n"
        "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);
}

/*TEST_F(ContextTest, VarDecInvalid)
{
    SetUp("double/vardec_invalid/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program \n";

    ASSERT_MLSTREQ(expected, symbols_string);
}*/

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

TEST_F(ContextTest, NoVarDecsFor)
{
    SetUpNoExecute("for/main.cvc");
    ASSERT_EXIT(
        run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
        testing::AllOf(testing::HasSubstr("was not declared"), testing::HasSubstr("13 Error")));
}

TEST_F(ContextTest, DoubleDec)
{
    SetUpNoExecute("double/vardec/main.cvc");
    ASSERT_EXIT(run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
                testing::AllOf(testing::HasSubstr("already defined"),
                               testing::HasSubstr("'value1'"), testing::HasSubstr("3:5 - 3:20")));
}

TEST_F(ContextTest, DoubleFunDef)
{
    SetUpNoExecute("double/fundef/main.cvc");
    ASSERT_EXIT(run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
                testing::AllOf(testing::HasSubstr("already defined"),
                               testing::HasSubstr("'fun_one_int_int'"),
                               testing::HasSubstr("1:1 - 1:23")));
}

/*TEST_F(ContextTest, VarDecInvalid)
{
    SetUpNoExecute("double/vardec_invalid/main.cvc");
    ASSERT_EXIT(run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
                testing::AllOf(testing::HasSubstr("not allowed"),
                               testing::HasSubstr("'_c'"),
                               testing::HasSubstr("5:1 - 1:23")));
}*/
