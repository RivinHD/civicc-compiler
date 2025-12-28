#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct globals
{
    const char *input_file;
    const char *output_file;
    char *input_buf;  // Buffer holding the input to the compiler if empty the input file is
                      // read instead.
    char *output_buf; // Buffer holding the output to the compiler if empty the output file is
                      // read instead.
    int line;
    int col;
    int verbose;
    uint32_t input_buf_len;
    uint32_t output_buf_len;
};

extern struct globals global;
extern void GLBinitializeGlobals(void);
