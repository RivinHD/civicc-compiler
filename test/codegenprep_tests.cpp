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

class CodeGenPrepTest : public testing::Test
{
  public:
    char *root_string = nullptr;
    char *symbols_string = nullptr;
    node_st *root = nullptr;
    std::string input_filepath;
    std::string err_output;
    std::string std_output;

  protected:
    CodeGenPrepTest()
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
        root = run_code_gen_preparation(input_filepath.c_str());
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

TEST_F(CodeGenPrepTest, InitFun)
{
    SetUp("milestone_6/init_fun/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'@temp_5'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'@temp_4'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'@temp_3'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'@temp_2'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'@temp_1'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'@temp_0'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'aa'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ Var -- name:'ab'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'float'\n"
                           "┃        ├─ Var -- name:'ba'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'float'\n"
                           "┃        ├─ Var -- name:'bb'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'bool'\n"
                           "┃        ├─ Var -- name:'ca'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'bool'\n"
                           "┃        ├─ Var -- name:'cb'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_0'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'da'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_1'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'db'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'float'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_2'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'ea'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'float'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_3'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'eb'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'bool'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_4'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'fa'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'0'\n"
                           "┃     └─ VarDec -- type:'bool'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_5'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'fb'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'__init'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'fb'\n"
                           "┃        ┃     └─ ProcCall\n"
                           "┃        ┃        ├─ Var -- name:'@alloc'\n"
                           "┃        ┃        ┢─ Exprs\n"
                           "┃        ┃        ┃  └─ Var -- name:'@temp_5'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'fa'\n"
                           "┃        ┃     └─ ProcCall\n"
                           "┃        ┃        ├─ Var -- name:'@alloc'\n"
                           "┃        ┃        ┢─ Exprs\n"
                           "┃        ┃        ┃  └─ Var -- name:'@temp_4'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ ForLoop\n"
                           "┃        ┃     ├─ Assign\n"
                           "┃        ┃     │  ├─ Var -- name:'@loop_var0'\n"
                           "┃        ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_4'\n"
                           "┃        ┃     ├─ NULL\n"
                           "┃        ┃     ┢─ Statements\n"
                           "┃        ┃     ┃  └─ ArrayAssign\n"
                           "┃        ┃     ┃     ├─ ArrayExpr\n"
                           "┃        ┃     ┃     │  ┢─ Exprs\n"
                           "┃        ┃     ┃     │  ┃  └─ Var -- name:'@loop_var0'\n"
                           "┃        ┃     ┃     │  ┡─ NULL\n"
                           "┃        ┃     ┃     │  └─ Var -- name:'fa'\n"
                           "┃        ┃     ┃     └─ Bool -- val:'1'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'eb'\n"
                           "┃        ┃     └─ ProcCall\n"
                           "┃        ┃        ├─ Var -- name:'@alloc'\n"
                           "┃        ┃        ┢─ Exprs\n"
                           "┃        ┃        ┃  └─ Var -- name:'@temp_3'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'ea'\n"
                           "┃        ┃     └─ ProcCall\n"
                           "┃        ┃        ├─ Var -- name:'@alloc'\n"
                           "┃        ┃        ┢─ Exprs\n"
                           "┃        ┃        ┃  └─ Var -- name:'@temp_2'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ ForLoop\n"
                           "┃        ┃     ├─ Assign\n"
                           "┃        ┃     │  ├─ Var -- name:'@loop_var0'\n"
                           "┃        ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_2'\n"
                           "┃        ┃     ├─ NULL\n"
                           "┃        ┃     ┢─ Statements\n"
                           "┃        ┃     ┃  └─ ArrayAssign\n"
                           "┃        ┃     ┃     ├─ ArrayExpr\n"
                           "┃        ┃     ┃     │  ┢─ Exprs\n"
                           "┃        ┃     ┃     │  ┃  └─ Var -- name:'@loop_var0'\n"
                           "┃        ┃     ┃     │  ┡─ NULL\n"
                           "┃        ┃     ┃     │  └─ Var -- name:'ea'\n"
                           "┃        ┃     ┃     └─ Float -- val:'5.000000'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'db'\n"
                           "┃        ┃     └─ ProcCall\n"
                           "┃        ┃        ├─ Var -- name:'@alloc'\n"
                           "┃        ┃        ┢─ Exprs\n"
                           "┃        ┃        ┃  └─ Var -- name:'@temp_1'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'da'\n"
                           "┃        ┃     └─ ProcCall\n"
                           "┃        ┃        ├─ Var -- name:'@alloc'\n"
                           "┃        ┃        ┢─ Exprs\n"
                           "┃        ┃        ┃  └─ Var -- name:'@temp_0'\n"
                           "┃        ┃        ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ ForLoop\n"
                           "┃        ┃     ├─ Assign\n"
                           "┃        ┃     │  ├─ Var -- name:'@loop_var0'\n"
                           "┃        ┃     │  └─ Int -- val:'0'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_0'\n"
                           "┃        ┃     ├─ NULL\n"
                           "┃        ┃     ┢─ Statements\n"
                           "┃        ┃     ┃  └─ ArrayAssign\n"
                           "┃        ┃     ┃     ├─ ArrayExpr\n"
                           "┃        ┃     ┃     │  ┢─ Exprs\n"
                           "┃        ┃     ┃     │  ┃  └─ Var -- name:'@loop_var0'\n"
                           "┃        ┃     ┃     │  ┡─ NULL\n"
                           "┃        ┃     ┃     │  └─ Var -- name:'da'\n"
                           "┃        ┃     ┃     └─ Int -- val:'4'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'ca'\n"
                           "┃        ┃     └─ Bool -- val:'1'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'ba'\n"
                           "┃        ┃     └─ Float -- val:'3.000000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'aa'\n"
                           "┃        ┃     └─ Int -- val:'2'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";
    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program\n"
               "├─ __init: FunDef -- has_export:'0'\n"
               "├─ aa: VarDec -- type:'int'\n"
               "├─ ab: VarDec -- type:'int'\n"
               "├─ ba: VarDec -- type:'float'\n"
               "├─ bb: VarDec -- type:'float'\n"
               "├─ ca: VarDec -- type:'bool'\n"
               "├─ cb: VarDec -- type:'bool'\n"
               "├─ da: VarDec -- type:'int'\n"
               "├─ db: VarDec -- type:'int'\n"
               "├─ ea: VarDec -- type:'float'\n"
               "├─ eb: VarDec -- type:'float'\n"
               "├─ fa: VarDec -- type:'bool'\n"
               "├─ fb: VarDec -- type:'bool'\n"
               "└────────────────────\n";
    ASSERT_MLSTREQ(expected, symbols_string);
}

/*TEST_F(CodeGenPrepTest, GlobalArrayAssignments)
{
    SetUp("milestone_6/global_defs/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'1'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_0'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'q'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'1'\n"
                           "┃     └─ VarDec -- type:'bool'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_1'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'test'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ GlobalDef -- has_export:'1'\n"
                           "┃     └─ VarDec -- type:'int'\n"
                           "┃        ├─ ArrayExpr\n"
                           "┃        │  ┢─ Exprs\n"
                           "┃        │  ┃  └─ Var -- name:'@temp_2'\n"
                           "┃        │  ┡─ NULL\n"
                           "┃        │  └─ Var -- name:'a'\n"
                           "┃        └─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'0'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'__init'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_0'\n"
                           "┃        ┃     └─ Int -- val:'3'\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_2'\n"
                           "┃        ┃     └─ Int -- val:'2'\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_1'\n"
                           "┃        ┃     └─ Binop -- op:'+'\n"
                           "┃        ┃        ├─ Int -- val:'3'\n"
                           "┃        ┃        └─ Int -- val:'1'\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@temp_0'\n"
                           "┃        ┃     └─ Int -- val:'1'\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ ProcCall\n"
                           "┃        ┃     ├─ Var -- name:'@alloc'\n"
                           "┃        ┃     ┢─ Exprs\n"
                           "┃        ┃     ┃  └─ Var -- name:'@temp_2'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ ProcCall\n"
                           "┃        ┃     ├─ Var -- name:'@alloc'\n"
                           "┃        ┃     ┢─ Exprs\n"
                           "┃        ┃     ┃  └─ Var -- name:'@temp_1'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ ProcCall\n"
                           "┃        ┃     ├─ Var -- name:'@alloc'\n"
                           "┃        ┃     ┢─ Exprs\n"
                           "┃        ┃     ┃  └─ Var -- name:'@temp_0'\n"
                           "┃        ┃     ┗─ NULL\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ ArrayAssign\n"
                           "┃        ┃     ├─ ArrayExpr\n"
                           "┃        ┃     │  ┢─ Exprs\n"
                           "┃        ┃     │  ┃  └─ Int -- val:'2'\n"
                           "┃        ┃     │  ┡─ NULL\n"
                           "┃        ┃     │  └─ Var -- name:'a'\n"
                           "┃        ┃     └─ Var -- name:'@temp_0'\n"
                           "┃        ┗─ NULL\n"
                           "┗─ NULL\n";
    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program\n"
               "├─ a: VarDec -- type:'int'\n"
               "├─ q: VarDec -- type:'int'\n"
               "├─ __init: FunDef -- has_export:'0'\n"
               "├─ test: VarDec -- type:'bool'\n"
               "└────────────────────\n";
    ASSERT_MLSTREQ(expected, symbols_string);
}*/

// /////////////////////////
// COMPILATION FAILURE tests
// /////////////////////////

// TEST_F(CodeGenPrepTest, TestName)
// {
//     SetUpNoExecute("path/main.cvc");
//     ASSERT_EXIT(
//         run_context_analysis(input_filepath.c_str()), testing::ExitedWithCode(1),
//         testing::AllOf(testing::HasSubstr("already defined"), testing::HasSubstr("1 Error")));
// }
