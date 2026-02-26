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

#ifdef __APPLE__
#define check_skip(val)                                                                            \
    if ((val))                                                                                     \
    {                                                                                              \
        GTEST_SKIP() << "Missing civas, civvm or gtimeout to execute. Test is skipped!";           \
    }
#else
#define check_skip(val)                                                                            \
    if ((val))                                                                                     \
    {                                                                                              \
        GTEST_SKIP() << "Missing civas, civvm or timeout to execute. Test is skipped!";            \
    }
#endif // __APPLE__

template <size_t TCount, bool TOptimize> class BehaviorTest : public testing::Test
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
    int vm_status;
    bool skipped = false;

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
        const path output_folder =
            project_directory / "build/test_output" /
            (test_info->name() + std::string(TOptimize ? "_Opt_" : "__") + std::to_string(TCount));
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
            root[i] = run_code_generation_file(input_filepath[i].c_str(),
                                               output_filepath[i].c_str(), TOptimize);
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
#if defined(PROGRAM_CIVVM) && defined(PROGRAM_CIVAS) && defined(HAS_PROG_TIMEOUT)
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
        vm_status = exit_status;

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

        for (std::string line; std::getline(stream, line);)
        {
            size_t start = line.find("Instructions executed: ");
            if (start != std::string::npos)
            {
                size_t size = std::strtoul(line.substr(start + 23).c_str(), nullptr, 10);
                EXPECT_NE(ERANGE, errno);
                EXPECT_NE(size, 0);
                instruction_count = size;
                break;
            }
        }
#else
        skipped = true;
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
class BehaviorTest_1 : public BehaviorTest<1, false>
{
};

class BehaviorTest_2 : public BehaviorTest<2, false>
{
};

class BehaviorTest_3 : public BehaviorTest<3, false>
{
};

class BehaviorTest_4 : public BehaviorTest<4, false>
{
};

class BehaviorTestOpt_1 : public BehaviorTest<1, true>
{
};

class BehaviorTestOpt_2 : public BehaviorTest<2, true>
{
};

class BehaviorTestOpt_3 : public BehaviorTest<3, true>
{
};

class BehaviorTestOpt_4 : public BehaviorTest<4, true>
{
};

TEST_F(BehaviorTest_1, WhileLoops)
{
    std::string filepaths[] = {"codegen/while_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(166, code_sizes[0]);

    ASSERT_EQ(1698, instruction_count);
}

TEST_F(BehaviorTest_1, ArrayInit)
{
    std::string filepaths[] = {"codegen/array_init/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(466, code_sizes[0]);

    ASSERT_EQ(237, instruction_count);
}

TEST_F(BehaviorTest_1, Binops)
{
    std::string filepaths[] = {"codegen/binops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(183, code_sizes[0]);

    ASSERT_EQ(43, instruction_count);
}

TEST_F(BehaviorTest_1, Casts)
{
    std::string filepaths[] = {"codegen/casts/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(89, code_sizes[0]);

    ASSERT_EQ(14, instruction_count);
}

TEST_F(BehaviorTest_1, DoWhileLoops)
{
    std::string filepaths[] = {"codegen/do_while_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(166, code_sizes[0]);

    ASSERT_EQ(2517, instruction_count);
}

TEST_F(BehaviorTest_1, ForLoops)
{
    std::string filepaths[] = {"codegen/for_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(174, code_sizes[0]);

    ASSERT_EQ(3384, instruction_count);
}

TEST_F(BehaviorTest_1, Monops)
{
    std::string filepaths[] = {"codegen/monops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(118, code_sizes[0]);

    ASSERT_EQ(23, instruction_count);
}

TEST_F(BehaviorTest_2, Suite_Arrays_ExternArrayArg)
{
    std::string filepaths[] = {"testsuite_public/arrays/check_success/extern_array_arg.cvc",
                               "testsuite_public/arrays/check_success/export_array_arg.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(92, code_sizes[0]);
    ASSERT_EQ(225, code_sizes[1]);

    ASSERT_EQ(201, instruction_count);
}

TEST_F(BehaviorTest_2, Suite_Arrays_Scopes)
{
    std::string filepaths[] = {"testsuite_public/arrays/check_success/scopes.cvc",
                               "testsuite_public/arrays/check_success/export_scopes.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(255, vm_status);

    ASSERT_EQ(318, code_sizes[0]);
    ASSERT_EQ(112, code_sizes[1]);

    ASSERT_THAT(vm_output, testing::HasSubstr("Exception: stack element is not an integer value"));
}

TEST_F(BehaviorTest_2, Suite_Arrays_CombinedExternArray)
{
    std::string filepaths[] = {"testsuite_public/arrays/combined_extern_array/main.cvc",
                               "testsuite_public/arrays/combined_extern_array/defs.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(151, code_sizes[0]);
    ASSERT_EQ(121, code_sizes[1]);

    ASSERT_EQ(48, instruction_count);

    ASSERT_THAT(vm_output, testing::HasSubstr("\n1235Instructions "));
}

TEST_F(BehaviorTest_1, Suite_Arrays_ArrayInit)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/array_init.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(362, code_sizes[0]);

    ASSERT_EQ(451, instruction_count);

    const char *expected = "1 2 3 \n"
                           "4 5 6 \n"
                           "\n"
                           "7 7 7 \n"
                           "7 7 7 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Arrays_ArrayInitGlobal)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/array_init_global.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(275, code_sizes[0]);

    ASSERT_EQ(164, instruction_count);

    const char *expected = "1 2 \n"
                           "3 4 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Arrays_ArraySingleEval)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/array_single_eval.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(551, code_sizes[0]);

    ASSERT_EQ(339, instruction_count);

    const char *expected = "1 1\n"
                           "\n"
                           "1 1\n"
                           "2 2\n"
                           "\n"
                           "2 4\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Arrays_Dimreduce)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/dimreduce.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(269, code_sizes[0]);

    ASSERT_EQ(236, instruction_count);

    const char *expected = "4 4 4 4 4 \n"
                           "1 2 3 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Arrays_Scopes)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/scopes.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(396, code_sizes[0]);

    ASSERT_EQ(228, instruction_count);

    const char *expected = "17\n"
                           "2\n"
                           "9\n"
                           "1\n"
                           " 4 5 6\n"
                           "1\n"
                           "2\n"
                           " 4 5 6\n"
                           "2\n"
                           "9\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_IntegerOutOfRange)
{
    std::string filepaths[] = {
        "testsuite_public/basic/check_error/integer_out_of_range.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(94, code_sizes[0]);

    ASSERT_EQ(8, instruction_count);

    const char *expected = "2147483647";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_2, Suite_Basic_CombinedExternVar)
{
    std::string filepaths[] = {"testsuite_public/basic/combined_extern_var/main.cvc",
                               "testsuite_public/basic/combined_extern_var/defs.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(108, code_sizes[0]);
    ASSERT_EQ(64, code_sizes[1]);

    ASSERT_EQ(9, instruction_count);

    const char *expected = "222";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_BoolOp)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/bool_op.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(681, code_sizes[0]);

    ASSERT_EQ(711, instruction_count);

    const char *expected = "100\n"
                           "010\n"
                           "111\n"
                           "001\n"
                           "\n"
                           "101\n"
                           "011\n"
                           "110\n"
                           "000\n"
                           "\n"
                           "100\n"
                           "00\n"
                           "111\n"
                           "00\n"
                           "\n"
                           "11\n"
                           "011\n"
                           "11\n"
                           "000\n"
                           "\n"
                           "100\n"
                           "010\n"
                           "111\n"
                           "000\n"
                           "\n"
                           "101\n"
                           "011\n"
                           "111\n"
                           "000\n"
                           "\n"
                           "1\n"
                           "0\n"
                           "\n"
                           "1\n"
                           "0\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_ForToWhile)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/for_to_while.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(403, code_sizes[0]);

    ASSERT_EQ(904, instruction_count);

    const char *expected = "0 1 2 3 4 5 6 7 8 9 \n"
                           "10 9 8 7 6 5 4 3 2 1 \n"
                           "0 2 4 6 8 \n"
                           "7 4 1 \n"
                           "0 0 0 1 0 2 0 3 \n"
                           "1 0 1 1 1 2 1 3 \n"
                           "2 0 2 1 2 2 2 3 \n"
                           "3 0 3 1 3 2 3 3 \n"
                           "4 0 4 1 4 2 4 3 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_ForLoop)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/forloop.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(306, code_sizes[0]);

    ASSERT_EQ(201, instruction_count);

    const char *expected = "9   8   7   6   5   \n"
                           "4   3   2   1   0   \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_GlobalInit)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/global_init.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(94, code_sizes[0]);

    ASSERT_EQ(8, instruction_count);

    const char *expected = "10";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_RecursionEarlyReturn)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/recursion_early_return.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(174, code_sizes[0]);

    ASSERT_EQ(313, instruction_count);

    const char *expected = "1 1 2 6 24 120 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_SimpleFor)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/simple_for.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(156, code_sizes[0]);

    ASSERT_EQ(137, instruction_count);

    const char *expected = "0 1 2 3 4 5 6 7 8 9 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_Typecheck)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/typecheck.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(370, code_sizes[0]);

    ASSERT_EQ(100, instruction_count);

    const char *expected = "0\n"
                           "4\n"
                           "35\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_Basic_VarInit)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/var_init.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(248, code_sizes[0]);

    ASSERT_EQ(73, instruction_count);

    const char *expected = "3\n"
                           "3\n"
                           "10\n"
                           "2\n"
                           "1\n"
                           "10\n"
                           "10\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_NestedFuns_NestedFuns)
{
    std::string filepaths[] = {
        "testsuite_public/nested_funs/functional/nested_funs.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(184, code_sizes[0]);

    ASSERT_EQ(69, instruction_count);

    const char *expected = "3215454454";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, Suite_NestedFuns_Scopse)
{
    std::string filepaths[] = {
        "testsuite_public/nested_funs/functional/scopes.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(356, code_sizes[0]);

    ASSERT_EQ(112, instruction_count);

    const char *expected = "11111\n"
                           "123\n"
                           "\n"
                           "22222\n"
                           "123\n"
                           "\n"
                           "33333\n"
                           "123\n"
                           "\n"
                           "11234\n"
                           "123\n"
                           "\n"
                           "123\n"
                           "\n"
                           "55555\n"
                           "\n"
                           "123\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_1, ArrayIndexPrint)
{
    std::string filepaths[] = {"array_index/main.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(201, code_sizes[0]);

    ASSERT_EQ(409, instruction_count);

    const char *expected = "2\n"
                           "3\n"
                           "4\n"
                           "5\n"
                           "6\n"
                           "7\n"
                           "8\n"
                           "9\n"
                           "10\n"
                           "11\n"
                           "12\n"
                           "13\n"
                           "14\n"
                           "15\n"
                           "16\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_3, Functional_Assignment01_Test)
{
    std::string filepaths[] = {"functional/assignment_01/test.cvc",
                               "functional/assignment_01/array.cvc",
                               "functional/assignment_01/core.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(1853, code_sizes[0]);
    ASSERT_EQ(1499, code_sizes[1]);
    ASSERT_EQ(450, code_sizes[2]);

    ASSERT_EQ(6473, instruction_count);

    const char *expected = "25 25\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_4, Functional_Assignment01_Queens)
{
    std::string filepaths[] = {
        "functional/assignment_01/queens.cvc", "functional/assignment_01/array.cvc",
        "functional/assignment_01/coreio.cvc", "functional/assignment_01/core.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(1, vm_status);

    ASSERT_EQ(1847, code_sizes[0]);
    ASSERT_EQ(1499, code_sizes[1]);
    ASSERT_EQ(514, code_sizes[2]);
    ASSERT_EQ(450, code_sizes[3]);

    ASSERT_EQ(61536242, instruction_count);

    const char *expected = "1\n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "0 0 0 0 0 1 0 0 \n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "0 1 0 0 0 0 0 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "\n"
                           "1\n"
                           "0 1 0 0 \n"
                           "0 0 0 1 \n"
                           "1 0 0 0 \n"
                           "0 0 1 0 \n"
                           "\n"
                           "0\n"
                           "0 0 0 \n"
                           "0 0 0 \n"
                           "0 0 0 \n"
                           "\n"
                           "1\n"
                           "1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 \n"
                           "0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 \n"
                           "0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 \n"
                           "0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n"
                           "\n"
                           "0\n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "0 0 0 0 0 0 0 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "\n"
                           "1\n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 1 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 1 0 0 \n"
                           "\n"
                           "1\n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 1 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 1 0 0 \n"
                           "\n"
                           "\n"
                           "\n"
                           "0\n"
                           "0\n"
                           "\n"
                           "-1\n"
                           "0\n"
                           "\n"
                           "30\n"
                           "1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987 1597 2584 4181 6765 "
                           "10946 17711 28657 46368 75025 121393 196418 317811 514229 832040 \n"
                           "\n"
                           "\n"
                           "\n"
                           "20\n"
                           "2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 \n"
                           "\n"
                           "0\n"
                           "\n"
                           "\n"
                           "-1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "12 123 12 3 23 12 -1 \n"
                           "12.000000 12.000000 32.000000 0.000000 -1.000000 \n"
                           "3 3 3 \n"
                           "1 2 3 \n"
                           "5 7 9 \n"
                           "\n"
                           "3.0000003.0000003.000000\n"
                           "1.0000002.0000003.000000\n"
                           "5.0000007.0000009.000000\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTest_2, Functional_Codegen_Paths)
{
    std::string filepaths[] = {"functional/codegen_paths/main.cvc",
                               "functional/codegen_paths/exports.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(2057, code_sizes[0]);
    ASSERT_EQ(1409, code_sizes[1]);

    ASSERT_EQ(30003895, instruction_count);

    const char *expected =
        "896091\n"
        "1\n"
        "\n"
        "\n"
        "460.986740\n"
        "\n"
        "1\n"
        "\n"
        "23\n"
        "\n"
        "3\n"
        "\n"
        "1\n"
        "1\n"
        "1\n"
        "1\n"
        "1\n"
        "0\n"
        "0\n"
        "0\n"
        "1\n"
        "3.140000\n"
        "7.140000\n"
        "7.140000\n"
        "0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  "
        "25  26  27  28  29  30  31  32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  "
        "47  48  49  50  51  52  53  54  55  56  57  58  59  60  61  62  63  64  65  66  67  68  "
        "69  70  71  72  73  74  75  76  77  78  79  80  81  82  83  84  85  86  87  88  89  90  "
        "91  92  93  94  95  96  97  98  99  100  101  102  103  104  105  106  107  108  109  110 "
        " 111  112  113  114  115  116  117  118  119  120  121  \n"
        "0  12\n"
        "0  9\n"
        "0  6\n"
        "0\n"
        "\n"
        "3  12\n"
        "3  9\n"
        "3  6\n"
        "3\n"
        "\n"
        "6  12\n"
        "6  9\n"
        "6  6\n"
        "6\n"
        "\n"
        "9  12\n"
        "9  9\n"
        "9  6\n"
        "9\n"
        "\n"
        "12\n"
        "\n"
        "11\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

// ===================================================
//            Tests including optimizations
// ===================================================

TEST_F(BehaviorTestOpt_1, WhileLoops)
{
    std::string filepaths[] = {"codegen/while_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_1, ArrayInit)
{
    std::string filepaths[] = {"codegen/array_init/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_1, Binops)
{
    std::string filepaths[] = {"codegen/binops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_1, Casts)
{
    std::string filepaths[] = {"codegen/casts/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_1, DoWhileLoops)
{
    std::string filepaths[] = {"codegen/do_while_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_1, ForLoops)
{
    std::string filepaths[] = {"codegen/for_loops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_1, Monops)
{
    std::string filepaths[] = {"codegen/monops/main.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);

    ASSERT_EQ(2, instruction_count);
}

TEST_F(BehaviorTestOpt_2, Suite_Arrays_ExternArrayArg)
{
    std::string filepaths[] = {"testsuite_public/arrays/check_success/extern_array_arg.cvc",
                               "testsuite_public/arrays/check_success/export_array_arg.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(43, code_sizes[0]);
    ASSERT_EQ(132, code_sizes[1]);

    ASSERT_EQ(193, instruction_count);
}

TEST_F(BehaviorTestOpt_2, Suite_Arrays_Scopes)
{
    std::string filepaths[] = {"testsuite_public/arrays/check_success/scopes.cvc",
                               "testsuite_public/arrays/check_success/export_scopes.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(255, vm_status);

    ASSERT_EQ(318, code_sizes[0]);
    ASSERT_EQ(81, code_sizes[1]);

    ASSERT_THAT(vm_output, testing::HasSubstr("Exception: array index out of bound"));
}

TEST_F(BehaviorTestOpt_2, Suite_Arrays_CombinedExternArray)
{
    std::string filepaths[] = {"testsuite_public/arrays/combined_extern_array/main.cvc",
                               "testsuite_public/arrays/combined_extern_array/defs.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(135, code_sizes[0]);
    ASSERT_EQ(121, code_sizes[1]);

    ASSERT_EQ(47, instruction_count);

    ASSERT_THAT(vm_output, testing::HasSubstr("\n1235Instructions "));
}

TEST_F(BehaviorTestOpt_1, Suite_Arrays_ArrayInit)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/array_init.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(306, code_sizes[0]);

    ASSERT_EQ(422, instruction_count);

    const char *expected = "1 2 3 \n"
                           "4 5 6 \n"
                           "\n"
                           "7 7 7 \n"
                           "7 7 7 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Arrays_ArrayInitGlobal)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/array_init_global.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(251, code_sizes[0]);

    ASSERT_EQ(146, instruction_count);

    const char *expected = "1 2 \n"
                           "3 4 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Arrays_ArraySingleEval)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/array_single_eval.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(422, code_sizes[0]);

    ASSERT_EQ(325, instruction_count);

    const char *expected = "1 1\n"
                           "\n"
                           "1 1\n"
                           "2 2\n"
                           "\n"
                           "2 4\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Arrays_Dimreduce)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/dimreduce.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(269, code_sizes[0]);

    ASSERT_EQ(236, instruction_count);

    const char *expected = "4 4 4 4 4 \n"
                           "1 2 3 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Arrays_Scopes)
{
    std::string filepaths[] = {
        "testsuite_public/arrays/functional/scopes.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(395, code_sizes[0]);

    ASSERT_EQ(227, instruction_count);

    const char *expected = "17\n"
                           "2\n"
                           "9\n"
                           "1\n"
                           " 4 5 6\n"
                           "1\n"
                           "2\n"
                           " 4 5 6\n"
                           "2\n"
                           "9\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_IntegerOutOfRange)
{
    std::string filepaths[] = {
        "testsuite_public/basic/check_error/integer_out_of_range.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(94, code_sizes[0]);

    ASSERT_EQ(8, instruction_count);

    const char *expected = "2147483647";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_2, Suite_Basic_CombinedExternVar)
{
    std::string filepaths[] = {"testsuite_public/basic/combined_extern_var/main.cvc",
                               "testsuite_public/basic/combined_extern_var/defs.cvc"};
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(72, code_sizes[0]);
    ASSERT_EQ(64, code_sizes[1]);

    ASSERT_EQ(8, instruction_count);

    const char *expected = "222";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_BoolOp)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/bool_op.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(611, code_sizes[0]);

    ASSERT_EQ(692, instruction_count);

    const char *expected = "100\n"
                           "010\n"
                           "111\n"
                           "001\n"
                           "\n"
                           "101\n"
                           "011\n"
                           "110\n"
                           "000\n"
                           "\n"
                           "100\n"
                           "00\n"
                           "111\n"
                           "00\n"
                           "\n"
                           "11\n"
                           "011\n"
                           "11\n"
                           "000\n"
                           "\n"
                           "100\n"
                           "010\n"
                           "111\n"
                           "000\n"
                           "\n"
                           "101\n"
                           "011\n"
                           "111\n"
                           "000\n"
                           "\n"
                           "1\n"
                           "0\n"
                           "\n"
                           "1\n"
                           "0\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_ForToWhile)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/for_to_while.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(386, code_sizes[0]);

    ASSERT_EQ(902, instruction_count);

    const char *expected = "0 1 2 3 4 5 6 7 8 9 \n"
                           "10 9 8 7 6 5 4 3 2 1 \n"
                           "0 2 4 6 8 \n"
                           "7 4 1 \n"
                           "0 0 0 1 0 2 0 3 \n"
                           "1 0 1 1 1 2 1 3 \n"
                           "2 0 2 1 2 2 2 3 \n"
                           "3 0 3 1 3 2 3 3 \n"
                           "4 0 4 1 4 2 4 3 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_ForLoop)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/forloop.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(234, code_sizes[0]);

    ASSERT_EQ(195, instruction_count);

    const char *expected = "9   8   7   6   5   \n"
                           "4   3   2   1   0   \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_GlobalInit)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/global_init.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(94, code_sizes[0]);

    ASSERT_EQ(8, instruction_count);

    const char *expected = "10";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_RecursionEarlyReturn)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/recursion_early_return.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(158, code_sizes[0]);

    ASSERT_EQ(312, instruction_count);

    const char *expected = "1 1 2 6 24 120 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_SimpleFor)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/simple_for.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(140, code_sizes[0]);

    ASSERT_EQ(136, instruction_count);

    const char *expected = "0 1 2 3 4 5 6 7 8 9 \n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_Typecheck)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/typecheck.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(264, code_sizes[0]);

    ASSERT_EQ(88, instruction_count);

    const char *expected = "0\n"
                           "4\n"
                           "35\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_Basic_VarInit)
{
    std::string filepaths[] = {
        "testsuite_public/basic/functional/var_init.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(238, code_sizes[0]);

    ASSERT_EQ(71, instruction_count);

    const char *expected = "3\n"
                           "3\n"
                           "10\n"
                           "2\n"
                           "1\n"
                           "10\n"
                           "10\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_NestedFuns_NestedFuns)
{
    std::string filepaths[] = {
        "testsuite_public/nested_funs/functional/nested_funs.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(156, code_sizes[0]);

    ASSERT_EQ(62, instruction_count);

    const char *expected = "3215454454";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, Suite_NestedFuns_Scopse)
{
    std::string filepaths[] = {
        "testsuite_public/nested_funs/functional/scopes.cvc",
    };
    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(322, code_sizes[0]);

    ASSERT_EQ(111, instruction_count);

    const char *expected = "11111\n"
                           "123\n"
                           "\n"
                           "22222\n"
                           "123\n"
                           "\n"
                           "33333\n"
                           "123\n"
                           "\n"
                           "11234\n"
                           "123\n"
                           "\n"
                           "123\n"
                           "\n"
                           "55555\n"
                           "\n"
                           "123\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_1, ArrayIndexPrint)
{
    std::string filepaths[] = {"array_index/main.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(201, code_sizes[0]);

    ASSERT_EQ(409, instruction_count);

    const char *expected = "2\n"
                           "3\n"
                           "4\n"
                           "5\n"
                           "6\n"
                           "7\n"
                           "8\n"
                           "9\n"
                           "10\n"
                           "11\n"
                           "12\n"
                           "13\n"
                           "14\n"
                           "15\n"
                           "16\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_3, Functional_Assignment01_Test)
{
    std::string filepaths[] = {"functional/assignment_01/test.cvc",
                               "functional/assignment_01/array.cvc",
                               "functional/assignment_01/core.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(1476, code_sizes[0]);
    ASSERT_EQ(1480, code_sizes[1]);
    ASSERT_EQ(433, code_sizes[2]);

    ASSERT_EQ(6334, instruction_count);

    const char *expected = "25 25\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_4, Functional_Assignment01_Queens)
{
    std::string filepaths[] = {
        "functional/assignment_01/queens.cvc", "functional/assignment_01/array.cvc",
        "functional/assignment_01/coreio.cvc", "functional/assignment_01/core.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(1, vm_status);

    ASSERT_EQ(1437, code_sizes[0]);
    ASSERT_EQ(1480, code_sizes[1]);
    ASSERT_EQ(399, code_sizes[2]);
    ASSERT_EQ(433, code_sizes[3]);

    ASSERT_EQ(61534591, instruction_count);

    const char *expected = "1\n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "0 0 0 0 0 1 0 0 \n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "0 1 0 0 0 0 0 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "\n"
                           "1\n"
                           "0 1 0 0 \n"
                           "0 0 0 1 \n"
                           "1 0 0 0 \n"
                           "0 0 1 0 \n"
                           "\n"
                           "0\n"
                           "0 0 0 \n"
                           "0 0 0 \n"
                           "0 0 0 \n"
                           "\n"
                           "1\n"
                           "1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 1 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 0 0 1 0 \n"
                           "0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 0 1 0 0 \n"
                           "0 0 0 0 0 0 0 0 1 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 0 0 0 0 1 \n"
                           "0 0 0 0 0 0 1 0 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 0 0 0 0 0 0 0 \n"
                           "\n"
                           "0\n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "0 0 0 0 0 0 0 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "\n"
                           "1\n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 1 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 1 0 0 \n"
                           "\n"
                           "1\n"
                           "0 0 1 0 0 0 0 0 \n"
                           "0 0 0 0 1 0 0 0 \n"
                           "0 1 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 0 1 \n"
                           "1 0 0 0 0 0 0 0 \n"
                           "0 0 0 0 0 0 1 0 \n"
                           "0 0 0 1 0 0 0 0 \n"
                           "0 0 0 0 0 1 0 0 \n"
                           "\n"
                           "\n"
                           "\n"
                           "0\n"
                           "0\n"
                           "\n"
                           "-1\n"
                           "0\n"
                           "\n"
                           "30\n"
                           "1 1 2 3 5 8 13 21 34 55 89 144 233 377 610 987 1597 2584 4181 6765 "
                           "10946 17711 28657 46368 75025 121393 196418 317811 514229 832040 \n"
                           "\n"
                           "\n"
                           "\n"
                           "20\n"
                           "2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 \n"
                           "\n"
                           "0\n"
                           "\n"
                           "\n"
                           "-1\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n"
                           "12 123 12 3 23 12 -1 \n"
                           "12.000000 12.000000 32.000000 0.000000 -1.000000 \n"
                           "3 3 3 \n"
                           "1 2 3 \n"
                           "5 7 9 \n"
                           "\n"
                           "3.0000003.0000003.000000\n"
                           "1.0000002.0000003.000000\n"
                           "5.0000007.0000009.000000\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}

TEST_F(BehaviorTestOpt_2, Functional_Codegen_Paths)
{
    std::string filepaths[] = {"functional/codegen_paths/main.cvc",
                               "functional/codegen_paths/exports.cvc"};

    SetUp(filepaths);
    ASSERT_NE(nullptr, root);
    Execute();
    check_skip(skipped);
    ASSERT_EQ(0, vm_status);

    ASSERT_EQ(1526, code_sizes[0]);
    ASSERT_EQ(1114, code_sizes[1]);

    ASSERT_EQ(30003041, instruction_count);

    const char *expected =
        "896091\n"
        "1\n"
        "\n"
        "\n"
        "460.986740\n"
        "\n"
        "1\n"
        "\n"
        "23\n"
        "\n"
        "3\n"
        "\n"
        "1\n"
        "1\n"
        "1\n"
        "1\n"
        "1\n"
        "0\n"
        "0\n"
        "0\n"
        "1\n"
        "3.140000\n"
        "7.140000\n"
        "7.140000\n"
        "0  1  2  3  4  5  6  7  8  9  10  11  12  13  14  15  16  17  18  19  20  21  22  23  24  "
        "25  26  27  28  29  30  31  32  33  34  35  36  37  38  39  40  41  42  43  44  45  46  "
        "47  48  49  50  51  52  53  54  55  56  57  58  59  60  61  62  63  64  65  66  67  68  "
        "69  70  71  72  73  74  75  76  77  78  79  80  81  82  83  84  85  86  87  88  89  90  "
        "91  92  93  94  95  96  97  98  99  100  101  102  103  104  105  106  107  108  109  110 "
        " 111  112  113  114  115  116  117  118  119  120  121  \n"
        "0  12\n"
        "0  9\n"
        "0  6\n"
        "0\n"
        "\n"
        "3  12\n"
        "3  9\n"
        "3  6\n"
        "3\n"
        "\n"
        "6  12\n"
        "6  9\n"
        "6  6\n"
        "6\n"
        "\n"
        "9  12\n"
        "9  9\n"
        "9  6\n"
        "9\n"
        "\n"
        "12\n"
        "\n"
        "11\n";

    ASSERT_THAT(vm_output, testing::HasSubstr(expected));
}
