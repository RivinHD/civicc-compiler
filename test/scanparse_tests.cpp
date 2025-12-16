#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <string>

extern "C"
{
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "ccngen/trav.h"
#include "to_string.h"
    node_st *run_scan_parse(const char *filepath);
    void cleanup_scan_parse(node_st *root);
}

class ScanParseTest : public testing::Test
{
  public:
    node_st *root = nullptr;
    std::string filepath;

  protected:
    ScanParseTest()
    {
    }

    void SetUp(std::string filename)
    {
        filepath =
            std::filesystem::absolute(std::string(PROJECT_DIRECTORY) + "/test/data/" + filename);
        ASSERT_TRUE(std::filesystem::exists(filepath))
            << "File does not exist at path '" << filepath << "'";
        root = run_scan_parse(filepath.c_str());
        EXPECT_NE(nullptr, root) << "Could not parse ast in file: '" << filepath << "'";
    }

    void TearDown() override
    {
        if (HasFailure())
        {
            char *root_string = node_to_string(root);
            std::cerr
                << "========================================================================\n"
                << "                        Node Representation\n"
                << "========================================================================\n"
                << root_string << std::endl;

            free(root_string);
        }

        cleanup_scan_parse(root);
        root = nullptr;
    }
};

TEST_F(ScanParseTest, ScanParse_GlobalDecBool)
{
    SetUp("globaldec_bool/main.cvc");
    ASSERT_NE(nullptr, root);

    ASSERT_EQ(root->nodetype, NT_PROGRAM);

    node_st *decls = PROGRAM_DECLS(root);
    ASSERT_EQ(NT_DECLARATIONS, decls->nodetype);
    node_st *dec1 = DECLARATIONS_DECL(decls);
    ASSERT_EQ(NT_GLOBALDEC, dec1->nodetype);
    ASSERT_EQ(DT_bool, GLOBALDEC_TYPE(dec1));
    node_st *var1 = GLOBALDEC_VAR(dec1);
    ASSERT_EQ(NT_VAR, var1->nodetype);
    ASSERT_STREQ("test", VAR_NAME(var1));

    decls = DECLARATIONS_NEXT(decls);
    ASSERT_EQ(NT_DECLARATIONS, decls->nodetype);
    node_st *dec2 = DECLARATIONS_DECL(decls);
    ASSERT_EQ(NT_GLOBALDEC, dec2->nodetype);
    ASSERT_EQ(DT_bool, GLOBALDEC_TYPE(dec2));
    node_st *array_var2 = GLOBALDEC_VAR(dec2);
    ASSERT_EQ(NT_ARRAYVAR, array_var2->nodetype);
    node_st *var2 = ARRAYVAR_VAR(array_var2);
    ASSERT_EQ(NT_VAR, var2->nodetype);
    ASSERT_STREQ("test1", VAR_NAME(var2));
    node_st *dims2 = ARRAYVAR_DIMS(array_var2);
    ASSERT_EQ(NT_DIMENSIONVARS, dims2->nodetype);
    node_st *var0_dim2 = DIMENSIONVARS_DIM(dims2);
    ASSERT_EQ(NT_VAR, var0_dim2->nodetype);
    ASSERT_STREQ("dim", VAR_NAME(var0_dim2));
    dims2 = DIMENSIONVARS_NEXT(dims2);
    ASSERT_EQ(nullptr, dims2);

    decls = DECLARATIONS_NEXT(decls);
    ASSERT_EQ(NT_DECLARATIONS, decls->nodetype);
    node_st *dec3 = DECLARATIONS_DECL(decls);
    ASSERT_EQ(NT_GLOBALDEC, dec3->nodetype);

    decls = DECLARATIONS_NEXT(decls);
    ASSERT_EQ(NT_DECLARATIONS, decls->nodetype);
    node_st *dec4 = DECLARATIONS_DECL(decls);
    ASSERT_EQ(NT_GLOBALDEC, dec4->nodetype);

    FAIL();
}

TEST_F(ScanParseTest, ScanParse_GlobalDecInt)
{
    SetUp("globaldec_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDecFloat)
{
    SetUp("globaldec_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDefBool)
{
    SetUp("globaldef_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDefInt)
{
    SetUp("globaldef_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDefFloat)
{
    SetUp("globaldef_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_FunDecBool)
{
    SetUp("fundec_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_FunDecInt)
{
    SetUp("fundec_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_FunDecFloat)
{
    SetUp("fundec_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_FunDefBool)
{
    SetUp("fundef_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_FunDefInt)
{
    SetUp("fundef_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_FunDefFloat)
{
    SetUp("fundef_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_LocalFundef)
{
    SetUp("local_fundef/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_LocalFundefs)
{
    SetUp("local_fundefs/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_LocalFundefsRecursive)
{
    SetUp("local_fundefs_recursive/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDefAssignArray)
{
    SetUp("globaldef_assign_array/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDefCast)
{
    SetUp("globaldef_cast/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_ProcCall)
{
    SetUp("proc_call/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_Binops)
{
    SetUp("binops/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_GlobalDecArray)
{
    SetUp("globaldec_array/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_DoWhile)
{
    SetUp("dowhile/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_If)
{
    SetUp("if/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_For)
{
    SetUp("for/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_Comment)
{
    SetUp("comments/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScanParseTest, ScanParse_BinopsPrecedence)
{
    SetUp("binops_precedence/main.cvc");
    ASSERT_NE(nullptr, root);
}
