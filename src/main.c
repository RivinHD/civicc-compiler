#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccn/dynamic_core.h"
#include "ccn/phase_driver.h"
#include "global/globals.h"
#include "palm/str.h"

static void Usage(char *program)
{
    char *program_bin = strrchr(program, '/');
    if (program_bin)
        program = program_bin + 1;

    printf("Usage: %s [OPTION...] <input file>\n", program);
    printf("Options:\n");
    printf("  --help/-h                    This help message.\n");
    printf("  --output/-o <output_file>    Output assembly to the given output file instead of "
           "STDOUT.\n");
    printf("  --nocpreprocessor/-ncpp      Disables the C preprocessor.\n");
    printf("  --nooptimization/-nopt       Disables the optimizations.\n");
}

static void RequiereArguments(char *option, int count, int argc, int index, char *program)
{
    if (index + count >= argc)
    {
        char *str_arg = count > 1 ? "arguments" : "argument";
        printf("ERROR: The '%s' option requieres %d positional %s.\n\n", option, count, str_arg);
        Usage(program);
        exit(EXIT_FAILURE);
    }
}

/* Parse command lines. Usages the globals struct to store data. */
static int ProcessArgs(int argc, char *argv[])
{
    int i_argc = 1;

    if (i_argc < argc)
    {
        char *arg = argv[i_argc];

        while (arg[0] == '-') // all options should start with a '-'
        {
            bool is_long = arg[1] == '-';
            if (STReq(arg, "-o") || (is_long && STReq(arg, "--output")))
            {
                RequiereArguments(arg, 1, argc, i_argc, argv[0]);
                global.output_file = argv[++i_argc];
            }
            else if (STReq(arg, "-ncpp") || (is_long && STReq(arg, "--nocpreprocessor")))
            {
                global.preprocessor_enabled = false;
            }
            else if (STReq(arg, "-nopt") || (is_long && STReq(arg, "--nooptimization")))
            {
                global.optimization_enabled = false;
            }
            else if (STReq(arg, "-h") || (is_long && STReq(arg, "--help")))
            {
                Usage(argv[0]);
                exit(EXIT_FAILURE);
            }
            else
            {
                printf("ERROR: Invalid option '%s'\n\n.", arg);
                Usage(argv[0]);
                exit(EXIT_FAILURE);
            }

            if (i_argc >= argc)
            {
                break;
            }

            arg = argv[++i_argc];
        }
    }

    if (i_argc == argc - 1)
    {
        global.input_file = argv[i_argc];
    }
    else
    {
        printf("ERROR: No civic file is given as last argument.\n\n.");
        Usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

// What to do when a breakpoint is reached.
void BreakpointHandler(node_st *root)
{
    TRAVstart(root, TRAV_PRT);
    return;
}

int main(int argc, char **argv)
{
    GLBinitializeGlobals();
    ProcessArgs(argc, argv);

    CCNrun(NULL);
    return 0;
}
