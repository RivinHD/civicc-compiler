#pragma once

#include <stdbool.h>
#include <stddef.h>

struct globals
{
    int line;
    int col;
    int verbose;
    int input_buf_len;
    char *input_buf; // Buffer holding the input to the compiler if empty the input file is
                     // read instead.
    const char *input_file;
    const char *output_file;
};

extern struct globals global;
extern void GLBinitializeGlobals(void);
