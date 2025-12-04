#include <filesystem>
#include <gtest/gtest.h>
#include <iostream>
#include <ostream>
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
        filepath = std::filesystem::absolute("test/data/" + filename);
        root = run_scanner(filepath.c_str());
        ASSERT_NE(nullptr, root) << "Could not parse ast in file: '" << filepath << "'";
    }

    void TearDown() override
    {
        cleanup_scanner(root);
        root = nullptr;
    }
};

TEST_F(ScannerTest, ScannerExample)
{
    SetUp("globaldef_bool/main.cvc");
    EXPECT_NE(nullptr, root);
}
