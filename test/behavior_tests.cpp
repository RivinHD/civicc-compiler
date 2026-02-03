#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdio.h>
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
    char *root_string[TCount];
    char *symbols_string[TCount];
    node_st *root[TCount];
    std::filesystem::path input_filepath[TCount];
    std::filesystem::path output_filepath[TCount];
    size_t code_sizes[TCount]; // in bytes; Filled after Execute was called.
    size_t instruction_count;
    std::string err_output;
    std::string std_output;
    std::string vm_output;

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
            ASSERT_TRUE(exists(input_filepath[i]))
                << "File does not exist at path '" << input_filepath[i] << "'";

            filepath.replace_extension(".s");
            output_filepath[i] = output_folder / filepath.filename();
        }
    }

    void SetUp(std::string (&filepaths)[TCount])
    {
        SetUpNoExecute(filepaths);
        testing::internal::CaptureStdout();
        testing::internal::CaptureStderr();
        for (size_t i = 0; i < TCount; i++)
        {
            root[i] =
                run_code_generation_file(input_filepath[i].c_str(), output_filepath[i].c_str());
            EXPECT_NE(nullptr, root) << "Could not parse ast in file: '" << input_filepath << "'";
            root_string[i] = node_to_string(root[i]);
            symbols_string[i] = symbols_to_string(root[i]);
        }
        err_output = testing::internal::GetCapturedStderr();
        std_output = testing::internal::GetCapturedStdout();
        ASSERT_THAT(err_output,
                    testing::Not(testing::HasSubstr("error: Inconsistent node found in AST")));
    }

    // Runs the generated civicc program and returns the number of exectued instructions.
    void Execute()
    {
#if defined(PROGRAM_CIVVM) && defined(PROGRAM_CIVAS)
        std::string objects;
        for (size_t i = 0; i < TCount; i++)
        {
            std::filesystem::path object_filepath{output_filepath[i]};
            object_filepath.replace_extension(".out");
            std::string assembler_cmd = PROGRAM_CIVAS + std::string(" 2>&1 -o ") +
                                        object_filepath.string() + " " +
                                        output_filepath[i].string();
            FILE *fd_civas = popen(assembler_cmd.c_str(), "r");
            ASSERT_NE(nullptr, fd_civas) << "Failed to popen civas";

            const size_t buf_len = 1024;
            char buf[buf_len];

            while (fgets(buf, buf_len, fd_civas) != NULL)
            {
                ASSERT_THAT(std::string(buf), testing::Not(testing::HasSubstr("error")));
            }

            // Close
            int status = pclose(fd_civas);

            ASSERT_NE(-1, status) << "Failed to retrieve the status";
            int exit_status = WEXITSTATUS(status);
            ASSERT_EQ(0, exit_status) << "Error on assembling the generated code.";

            int signal = WIFSIGNALED(status);
            ASSERT_EQ(0, signal) << "Killed by signal: '" << WTERMSIG(status) << "'";

            objects += " " + object_filepath.string();
        }

#ifdef __APPLE__
        std::string timeout_cmd = "gtimeout 3s ";
#else
        std::string timeout_cmd = "timeout 3s ";
#endif // __APPLE__
        std::string options = " 2>&1 --size --instrs ";
        std::string vm_cmd = timeout_cmd + PROGRAM_CIVVM + options + objects;
        FILE *fd_civvm = popen(vm_cmd.c_str(), "r");
        ASSERT_NE(nullptr, fd_civvm) << "Failed to popen civas";

        std::stringstream stream;

        const size_t buf_len = 1024;
        char buf[buf_len];

        while (fgets(buf, buf_len, fd_civvm) != NULL)
        {
            stream << buf;
            vm_output += buf;
        }

        // Close
        int status = pclose(fd_civvm);

        ASSERT_NE(-1, status) << "Failed to retrieve the status";
        int exit_status = WEXITSTATUS(status);
        ASSERT_NE(-1, exit_status) << "Failed to create child";
        ASSERT_NE(124, exit_status) << "Timeout!";
        ASSERT_EQ(0, exit_status) << "Error on running in the civvm.";

        int signal = WIFSIGNALED(status);
        ASSERT_EQ(0, signal) << "Killed by signal: '" << WTERMSIG(status) << "'";

        for (size_t i = 0; i < TCount; i++)
        {
            std::string line;
            std::getline(stream, line);
            size_t start = line.find(": ");
            size_t end = line.rfind(" bytes");
            size_t size = std::strtoul(line.substr(start + 1, end - start).c_str(), nullptr, 10);
            EXPECT_NE(ERANGE, errno);
            EXPECT_NE(size, 0);
            EXPECT_TRUE(size >= 28); // CivicC VM bytes code has a fixed 28 bytes infill
            code_sizes[i] = size;
        }

        std::string line;
        std::getline(stream, line);
        size_t start = line.find(": ");
        size_t size = std::strtoul(line.substr(start + 1).c_str(), nullptr, 10);
        EXPECT_NE(ERANGE, errno);
        EXPECT_NE(size, 0);
        instruction_count = size;
#else
        GTEST_SKIP() << "Missing civas or civvm to execute. Test is skipped!";
#endif // defined (PROGRAM_CIVVM) && defined (PROGRAM_CIVAS)
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
                              << "Node Representation of " << input_filepath[i] << "\n"
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
                              << "Symbol Tables of AST of " << input_filepath[i] << "\n"
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

        if (HasFailure())
        {
            std::cerr
                << "========================================================================\n"
                << "                      Compilation Standard Output\n"
                << "========================================================================\n"
                << std_output << std::endl;

            std::cerr
                << "========================================================================\n"
                << "                       Compilation Error Output\n"
                << "========================================================================\n"
                << err_output << std::endl;

            std::cerr
                << "========================================================================\n"
                << "                             VM Output\n"
                << "========================================================================\n"
                << vm_output << std::endl;
        }
    }
};

// Holds a civicc programm that contains 1 program.
class BehaviorTest_1 : public BehaviorTest<1>
{
};

class BehaviorTest_2 : public BehaviorTest<2>
{
};

TEST_F(BehaviorTest_1, WhileLoops)
{
    std::string filepaths[] = {"codegen/while_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(1698, instruction_count);

    ASSERT_EQ(166, code_sizes[0]);
}

TEST_F(BehaviorTest_1, ArrayInit)
{
    std::string filepaths[] = {"codegen/array_init/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(237, instruction_count);

    ASSERT_EQ(466, code_sizes[0]);
}

TEST_F(BehaviorTest_1, Binops)
{
    std::string filepaths[] = {"codegen/binops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(43, instruction_count);

    ASSERT_EQ(183, code_sizes[0]);
}

TEST_F(BehaviorTest_1, Casts)
{
    std::string filepaths[] = {"codegen/casts/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(14, instruction_count);

    ASSERT_EQ(89, code_sizes[0]);
}

TEST_F(BehaviorTest_1, DoWhileLoops)
{
    std::string filepaths[] = {"codegen/do_while_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(2517, instruction_count);

    ASSERT_EQ(166, code_sizes[0]);
}

TEST_F(BehaviorTest_1, ForLoops)
{
    std::string filepaths[] = {"codegen/for_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(3384, instruction_count);

    ASSERT_EQ(174, code_sizes[0]);
}

TEST_F(BehaviorTest_1, Monops)
{
    std::string filepaths[] = {"codegen/monops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(23, instruction_count);

    ASSERT_EQ(118, code_sizes[0]);
}

TEST_F(BehaviorTest_2, Suite_Arrays_ExternArrayArg)
{
    std::string filepaths[] = {"testsuite_public/arrays/check_success/extern_array_arg.cvc",
                               "testsuite_public/arrays/check_success/export_array_arg.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();

    ASSERT_EQ(201, instruction_count);

    ASSERT_EQ(92, code_sizes[0]);
    ASSERT_EQ(225, code_sizes[1]);
}
