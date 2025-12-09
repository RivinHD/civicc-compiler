#include <filesystem>
#include <gtest/gtest.h>
#include <memory_resource>
#include <string>

extern "C"
{
    struct node_st;
    node_st *run_scanner(const char *filepath);
    void cleanup_scanner(node_st *root);
}

class ScannerTest : public testing::Test
{
  public:
    node_st *root = nullptr;
    std::string filepath;

  protected:
    ScannerTest()
    {
    }

    void SetUp(std::string filename)
    {
        filepath =
            std::filesystem::absolute(std::string(PROJECT_DIRECTORY) + "/test/data/" + filename);
        ASSERT_TRUE(std::filesystem::exists(filepath))
            << "File does not exist at path '" << filepath << "'";
        root = run_scanner(filepath.c_str());
        EXPECT_NE(nullptr, root) << "Could not parse ast in file: '" << filepath << "'";
    }

    void TearDown() override
    {
        cleanup_scanner(root);
        root = nullptr;
    }
};

TEST_F(ScannerTest, Scan_GlobalDecBool)
{
    SetUp("globaldec_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDecInt)
{
    SetUp("globaldec_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDecFloat)
{
    SetUp("globaldec_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDefBool)
{
    SetUp("globaldef_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDefInt)
{
    SetUp("globaldef_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDefFloat)
{
    SetUp("globaldef_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_FunDecBool)
{
    SetUp("fundec_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_FunDecInt)
{
    SetUp("fundec_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_FunDecFloat)
{
    SetUp("fundec_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_FunDefBool)
{
    SetUp("fundef_bool/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_FunDefInt)
{
    SetUp("fundef_int/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_FunDefFloat)
{
    SetUp("fundef_float/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_LocalFundef)
{
    SetUp("local_fundef/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_LocalFundefs)
{
    SetUp("local_fundefs/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_LocalFundefsRecursive)
{
    SetUp("local_fundefs_recursive/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDefAssignArray)
{
    SetUp("globaldef_assign_array/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_GlobalDefCast)
{
    SetUp("globaldef_cast/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_ProcCall)
{
    SetUp("proc_call/main.cvc");
    ASSERT_NE(nullptr, root);
}

TEST_F(ScannerTest, Scan_Binops)
{
    SetUp("binops/main.cvc");
    ASSERT_NE(nullptr, root);
}
