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
                           "┃     │  ├─ Var -- name:'@fun_test'\n"
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

    expected = "┌─ 0: Program\n"
               "├─ @fun_test: FunHeader -- type:'void' -- Params: (null)\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '@fun_test' -- parent: '0: Program'\n"
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
                           "┃     │  ├─ Var -- name:'@fun_main'\n"
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

    expected = "┌─ 0: Program\n"
               "├─ @fun_main: FunHeader -- type:'void' -- Params: int (Var -- name:'a')\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '@fun_main' -- parent: '0: Program'\n"
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

TEST_F(ContextTest, SameIdentifierFunDefVarDecValid)
{
    SetUp("same_identifier/fundef_vardec/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'@fun_fun_one'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param0'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'void'\n"
                           "┃        ┃     │  ├─ Var -- name:'@fun_param0'\n"
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
                           "┃     │  ├─ Var -- name:'@fun_fun_two'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param1'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'void'\n"
                           "┃        ┃     │  ├─ Var -- name:'@fun_nested'\n"
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
                           "┃     │  ├─ Var -- name:'@fun_fun_three'\n"
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
                           "┃        ┃     │  ├─ Var -- name:'@fun_shadow'\n"
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

    expected = "┌─ 0: Program\n"
               "├─ @fun_fun_three: FunHeader -- type:'bool' -- Params: int (Var -- name:'param2')\n"
               "├─ @fun_fun_one: FunHeader -- type:'int' -- Params: int (Var -- name:'param0')\n"
               "├─ @fun_fun_two: FunHeader -- type:'float' -- Params: int (Var -- name:'param1')\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '@fun_fun_one' -- parent: '0: Program'\n"
               "├─ param0: Params -- type:'int'\n"
               "├─ @fun_param0: FunHeader -- type:'void' -- Params: (null)\n"
               "└────────────────────\n"
               "┌─ 2: FunDef '@fun_param0' -- parent: '1: FunDef '@fun_fun_one''\n"
               "├─ a: VarDec -- type:'int'\n"
               "└────────────────────\n"
               "┌─ 1: FunDef '@fun_fun_two' -- parent: '0: Program'\n"
               "├─ param1: Params -- type:'int'\n"
               "├─ @fun_nested: FunHeader -- type:'void' -- Params: int (Var -- name:'val1')\n"
               "└────────────────────\n"
               "┌─ 2: FunDef '@fun_nested' -- parent: '1: FunDef '@fun_fun_two''\n"
               "├─ a: VarDec -- type:'int'\n"
               "├─ val1: Params -- type:'int'\n"
               "├─ nested: VarDec -- type:'int'\n"
               "└────────────────────\n"
               "┌─ 1: FunDef '@fun_fun_three' -- parent: '0: Program'\n"
               "├─ param2: Params -- type:'int'\n"
               "├─ shadow: VarDec -- type:'int'\n"
               "├─ @fun_shadow: FunHeader -- type:'int' -- Params: int (Var -- name:'val1')\n"
               "└────────────────────\n"
               "┌─ 2: FunDef '@fun_shadow' -- parent: '1: FunDef '@fun_fun_three''\n"
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
                           "┃     │  ├─ Var -- name:'@fun_fun_one'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'param0'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ LocalFunDefs\n"
                           "┃        ┃  └─ FunDef -- has_export:'0'\n"
                           "┃        ┃     ├─ FunHeader -- type:'void'\n"
                           "┃        ┃     │  ├─ Var -- name:'@fun_nested_fun_one'\n"
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
        "┌─ 0: Program\n"
        "├─ @fun_fun_one: FunHeader -- type:'int' -- Params: int (Var -- name:'param0')\n"
        "└────────────────────\n"
        "\n"
        "┌─ 1: FunDef '@fun_fun_one' -- parent: '0: Program'\n"
        "├─ param0: Params -- type:'int'\n"
        "├─ @fun_nested_fun_one: FunHeader -- type:'void' -- Params: int (Var -- name:'param0')\n"
        "└────────────────────\n"
        "┌─ 2: FunDef '@fun_nested_fun_one' -- parent: '1: FunDef '@fun_fun_one''\n"
        "├─ param0: Params -- type:'int'\n"
        "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);
}

TEST_F(ContextTest, ProcCallContext)
{
    SetUp("proc_call/context/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'@fun_fun'\n"
                           "┃     │  ┢─ Params -- type:'int'\n"
                           "┃     │  ┃  └─ Var -- name:'i'\n"
                           "┃     │  ┗─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Var -- name:'i'\n"
                           "┃        ┗─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'@fun_test'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'b'\n"
                           "┃        ┃     └─ Binop -- op:'+'\n"
                           "┃        ┃        ├─ ProcCall\n"
                           "┃        ┃        │  ├─ Var -- name:'@fun_fun'\n"
                           "┃        ┃        │  ┢─ Exprs\n"
                           "┃        ┃        │  ┃  └─ Int -- val:'1'\n"
                           "┃        ┃        │  ┗─ NULL\n"
                           "┃        ┃        └─ ProcCall\n"
                           "┃        ┃           ├─ Var -- name:'@fun_fun'\n"
                           "┃        ┃           ┢─ Exprs\n"
                           "┃        ┃           ┃  └─ Int -- val:'2'\n"
                           "┃        ┃           ┗─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        └─ NULL\n"
                           "┗─ NULL\n";
    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program\n"
               "├─ @fun_test: FunHeader -- type:'void' -- Params: (null)\n"
               "├─ @fun_fun: FunHeader -- type:'int' -- Params: int (Var -- name:'i')\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '@fun_fun' -- parent: '0: Program'\n"
               "├─ i: Params -- type:'int'\n"
               "└────────────────────\n"
               "┌─ 1: FunDef '@fun_test' -- parent: '0: Program'\n"
               "├─ b: VarDec -- type:'int'\n"
               "└────────────────────\n";
    ASSERT_MLSTREQ(expected, symbols_string);
}

// /////////////////////////
// COMPILATION FAILURE tests
// /////////////////////////

TEST_F(ContextTest, DoubleFundefValid)
{
    SetUpNoExecute("double/fundef_valid/main.cvc");
    ASSERT_EXIT(
        run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
        testing::AllOf(testing::HasSubstr("already defined"), testing::HasSubstr("1 Error")));
}

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
    ASSERT_EXIT(run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
                testing::AllOf(testing::HasSubstr("was not declared"),
                               testing::HasSubstr(
                                   "'cast expression' has type 'float' but expected type 'int'"),
                               testing::HasSubstr("14 Error")));
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
                testing::AllOf(testing::HasSubstr("'fun_one' already defined"),
                               testing::HasSubstr("1:1 - 1:23")));
}

TEST_F(ContextTest, VarDecInvalid)
{
    SetUpNoExecute("double/vardec_invalid/main.cvc");
    ASSERT_EXIT(run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
                testing::AllOf(testing::HasSubstr("invalid character"),
                               testing::HasSubstr("line 7, col 8")));
}
