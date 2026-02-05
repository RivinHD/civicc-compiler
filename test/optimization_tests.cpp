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

class OptimizationTest : public testing::Test
{
  public:
    char *root_string = nullptr;
    char *symbols_string = nullptr;
    node_st *root = nullptr;
    std::string input_filepath;
    std::string err_output;
    std::string std_output;

  protected:
    OptimizationTest()
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
        root = run_optimization(input_filepath.c_str());
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

TEST_F(OptimizationTest, AlgebraicSimplifications)
{
    SetUp("optimization/algebraic_simplification/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'1'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'@fun_main'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_a'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_b'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_c'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_d'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_e'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_f'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_g1'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_g2'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_h1'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_h2'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_i'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_a'\n"
                           "┃        ┃     └─ Var -- name:'@1_a'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_b'\n"
                           "┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_c'\n"
                           "┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_d'\n"
                           "┃        ┃     └─ Int -- val:'14'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_e'\n"
                           "┃        ┃     └─ Int -- val:'4'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_f'\n"
                           "┃        ┃     └─ Binop -- op:'+'\n"
                           "┃        ┃        ├─ Var -- name:'@1_e'\n"
                           "┃        ┃        └─ Int -- val:'0'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_g1'\n"
                           "┃        ┃     └─ Float -- val:'0.000000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_g2'\n"
                           "┃        ┃     └─ Float -- val:'0.000000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_h1'\n"
                           "┃        ┃     └─ Var -- name:'@1_g1'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_h2'\n"
                           "┃        ┃     └─ Var -- name:'@1_g2'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_i'\n"
                           "┃        ┃     └─ Float -- val:'1.000000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┗─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'1'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'__init'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        └─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program\n"
               "├─ __init: FunHeader -- type:'void' -- Params: (null)\n"
               "├─ @fun_main: FunHeader -- type:'int' -- Params: (null)\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '@fun_main' -- parent: '0: Program'\n"
               "├─ @1_g1: VarDec -- type:'float'\n"
               "├─ @1_g2: VarDec -- type:'float'\n"
               "├─ @1_h1: VarDec -- type:'float'\n"
               "├─ @1_h2: VarDec -- type:'float'\n"
               "├─ @1_a: VarDec -- type:'int'\n"
               "├─ @1_b: VarDec -- type:'int'\n"
               "├─ @1_c: VarDec -- type:'int'\n"
               "├─ @1_d: VarDec -- type:'int'\n"
               "├─ @1_e: VarDec -- type:'int'\n"
               "├─ @1_f: VarDec -- type:'int'\n"
               "├─ @1_i: VarDec -- type:'float'\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '__init' -- parent: '0: Program'\n"
               "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);
}

TEST_F(OptimizationTest, ConstantFolding)
{
    SetUp("optimization/constant_folding/main.cvc");
    ASSERT_NE(nullptr, root);

    const char *expected = "Program\n"
                           "┢─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'1'\n"
                           "┃     ├─ FunHeader -- type:'int'\n"
                           "┃     │  ├─ Var -- name:'@fun_main'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ┢─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_a'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_b'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'int'\n"
                           "┃        ┃     ├─ Var -- name:'@1_c'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_d'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_e'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┣─ VarDecs\n"
                           "┃        ┃  └─ VarDec -- type:'float'\n"
                           "┃        ┃     ├─ Var -- name:'@1_f'\n"
                           "┃        ┃     └─ NULL\n"
                           "┃        ┡─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        ┢─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_a'\n"
                           "┃        ┃     └─ Int -- val:'17'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_b'\n"
                           "┃        ┃     └─ Binop -- op:'+'\n"
                           "┃        ┃        ├─ Var -- name:'@1_a'\n"
                           "┃        ┃        └─ Int -- val:'3'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_c'\n"
                           "┃        ┃     └─ Binop -- op:'+'\n"
                           "┃        ┃        ├─ Binop -- op:'*'\n"
                           "┃        ┃        │  ├─ Var -- name:'@1_a'\n"
                           "┃        ┃        │  └─ Int -- val:'2'\n"
                           "┃        ┃        └─ Int -- val:'3'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_d'\n"
                           "┃        ┃     └─ Float -- val:'7.720000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_e'\n"
                           "┃        ┃     └─ Binop -- op:'+'\n"
                           "┃        ┃        ├─ Binop -- op:'-'\n"
                           "┃        ┃        │  ├─ Float -- val:'4.400000'\n"
                           "┃        ┃        │  └─ Var -- name:'@1_e'\n"
                           "┃        ┃        └─ Float -- val:'3.000000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ Assign\n"
                           "┃        ┃     ├─ Var -- name:'@1_f'\n"
                           "┃        ┃     └─ Binop -- op:'*'\n"
                           "┃        ┃        ├─ Binop -- op:'+'\n"
                           "┃        ┃        │  ├─ Binop -- op:'*'\n"
                           "┃        ┃        │  │  ├─ Float -- val:'2.000000'\n"
                           "┃        ┃        │  │  └─ Var -- name:'@1_e'\n"
                           "┃        ┃        │  └─ Float -- val:'3.300000'\n"
                           "┃        ┃        └─ Float -- val:'5.600000'\n"
                           "┃        ┣─ Statements\n"
                           "┃        ┃  └─ RetStatement\n"
                           "┃        ┃     └─ Int -- val:'0'\n"
                           "┃        ┗─ NULL\n"
                           "┣─ Declarations\n"
                           "┃  └─ FunDef -- has_export:'1'\n"
                           "┃     ├─ FunHeader -- type:'void'\n"
                           "┃     │  ├─ Var -- name:'__init'\n"
                           "┃     │  └─ NULL\n"
                           "┃     └─ FunBody\n"
                           "┃        ├─ NULL\n"
                           "┃        ├─ NULL\n"
                           "┃        └─ NULL\n"
                           "┗─ NULL\n";

    ASSERT_MLSTREQ(expected, root_string);

    expected = "┌─ 0: Program\n"
               "├─ __init: FunHeader -- type:'void' -- Params: (null)\n"
               "├─ @fun_main: FunHeader -- type:'int' -- Params: (null)\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '@fun_main' -- parent: '0: Program'\n"
               "├─ @1_a: VarDec -- type:'int'\n"
               "├─ @1_b: VarDec -- type:'int'\n"
               "├─ @1_c: VarDec -- type:'int'\n"
               "├─ @1_d: VarDec -- type:'float'\n"
               "├─ @1_e: VarDec -- type:'float'\n"
               "├─ @1_f: VarDec -- type:'float'\n"
               "└────────────────────\n"
               "\n"
               "┌─ 1: FunDef '__init' -- parent: '0: Program'\n"
               "└────────────────────\n";

    ASSERT_MLSTREQ(expected, symbols_string);
}

// /////////////////////////
// COMPILATION FAILURE tests
// /////////////////////////

// TEST_F(OptimizationTest, DoubleArrParams)
// {
//     SetUpNoExecute("milestone_8/double_arr_params/main.cvc");
//     ASSERT_EXIT(
//         run_code_gen_preparation(input_filepath.c_str()), testing::ExitedWithCode(1),
//         testing::AllOf(testing::HasSubstr("already defined"), testing::HasSubstr("3 Error")));
// }
