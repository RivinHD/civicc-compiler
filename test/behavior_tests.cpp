#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

extern "C"
{
#include "test_interface.h"
#include "to_string.h"
}

template <size_t TCount> class BehaviorTest : public testing::Test
{
  public:
    char *root_string[TCount] = nullptr;
    char *symbols_string[TCount] = nullptr;
    node_st *root[TCount] = nullptr;
    std::filesystem::path input_filepath[TCount];
    std::filesystem::path output_filepath[TCount];
    size_t code_sizes[TCount]; // in bytes; Filled after Execute was called.
    std::string err_output;
    std::string std_output;

  protected:
    BehaviorTest()
    {
    }

    void SetUpNoExecute(std::string filepaths[TCount])
    {
        const testing::TestInfo *const test_info =
            testing::UnitTest::GetInstance()->current_test_info();

        using namespace std::filesystem;
        const path project_directory = std::filesystem::absolute(PROJECT_DIRECTORY);
        const path input_folder = project_directory / "test/data";
        const path output_folder = project_directory / "build/test_output" / test_info->name();
        create_directories(output_folder);
        for (size_t i = 0; i < TCount; i++)
        {
            path filepath = filepaths[i];
            input_filepath[i] = input_folder / filepath;
            ASSERT_TRUE(exists(input_filepath))
                << "File does not exist at path '" << input_filepath << "'";

            filepath.replace_extension(".s");
            output_filepath[i] = output_folder / filepath;
        }
    }

    void SetUp(std::string filepaths[TCount])
    {
        SetUpNoExecute(filepaths);
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        for (size_t i = 0; i < TCount; i++)
        {
            root[i] =
                run_code_generation_file(input_filepath[i].c_str(), output_filepath[i].c_str());
            EXPECT_NE(nullptr, root) << "Could not parse ast in file: '" << input_filepath << "'";
            root_string[i] = node_to_string(root);
            symbols_string[i] = symbols_to_string(root);
        }
        err_output = testing::internal::GetCapturedStderr();
        std_output = testing::internal::GetCapturedStdout();
        ASSERT_THAT(err_output,
                    testing::Not(testing::HasSubstr("error: Inconsistent node found in AST")));
    }

    // Runs the generated civicc program and returns the number of exectued instructions.
    size_t Execute()
    {
        std::string objects;
        for (size_t i = 0; i < TCount; i++)
        {
            std::filesystem::path object_filepath{output_filepath[i]};
            object_filepath.replace_extension(".out");
            std::string assembler_cmd = PROGRAM_CIVAS + std::string(" -o ") +
                                        object_filepath.string() + " " +
                                        output_filepath[i].string();
            system(assembler_cmd.c_str());

            objects += " " + object_filepath.string();
        }

        std::string timeout_cmd = "timeout 3s "; // Add
        std::string options = " --size --instrs ";
        std::string vm_cmd = timeout_cmd + PROGRAM_CIVVM + options + objects;
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        system(vm_cmd.c_str());
        err_output = testing::internal::GetCapturedStderr();
        std_output = testing::internal::GetCapturedStdout();

        std::istringstream stream(err_output);

        for (size_t i = 0; i < TCount; i++)
        {
            std::string line;
            std::getline(stream, line);
            size_t start = line.find(": ");
            size_t end = line.rfind(" bytes");
            size_t size = std::strtoul(line.substr(start + 1, end - start).c_str(), nullptr, 10);
            EXPECT_NE(ERANGE, errno);
            EXPECT_NE(size, 0);
            code_sizes[i] = size;
        }

        std::string line;
        std::getline(stream, line);
        size_t start = line.find(": ");
        size_t size = std::strtoul(line.substr(start + 1).c_str(), nullptr, 10);
        EXPECT_NE(ERANGE, errno);
        EXPECT_NE(size, 0);
        EXPECT_TRUE(size >= 28); // CivicC VM bytes code has a fixed 28 bytes infill
        size -= 28;
        return size;
    }

    void TearDown() override
    {
        for (size_t i = 0; i < TCount; i++)
        {
            if (root_string[i] != nullptr)
            {

                if (HasFailure())
                {
                    std::cerr << "================================================================="
                                 "=======\n"
                              << "                        Node Representation\n"
                              << "================================================================="
                                 "=======\n"
                              << root_string[i] << std::endl;
                }

                free(root_string[i]);
            }
            if (symbols_string[i] != nullptr)
            {
                if (HasFailure())
                {
                    std::cerr << "================================================================="
                                 "=======\n"
                              << "                        Symbol Tables of AST\n"
                              << "================================================================="
                                 "=======\n"
                              << symbols_string[i] << std::endl;
                }

                free(symbols_string[i]);
            }

            if (root[i] != nullptr)
            {
                cleanup_nodes(root[i]);
                root[i] = nullptr;
            }
        }
    }
};

// Holds a civicc programm that contains 1 program.
class BehaviorTest_1 : public BehaviorTest<1>
{
};
