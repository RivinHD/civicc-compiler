#include <filesystem>
#include <gtest/gtest.h>
#include <memory_resource>
#include <string>

extern "C"
{
    struct node_st;
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
        cleanup_scan_parse(root);
        root = nullptr;
    }
};

TEST_F(ScanParseTest, ScanParse_GlobalDecBool)
{
    SetUp("globaldec_bool/main.cvc");
    ASSERT_NE(nullptr, root);
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
